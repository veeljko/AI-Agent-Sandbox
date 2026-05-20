#include "krabs.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <exception>
#include <cwctype>
#include <unordered_map>


void stop_stale_trace_session(const wchar_t* session_name) {
    struct trace_properties_buffer {
        EVENT_TRACE_PROPERTIES properties;
        wchar_t logger_name[1024];
    };

    trace_properties_buffer buffer = {};
    buffer.properties.Wnode.BufferSize = sizeof(buffer);
    buffer.properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ControlTraceW(
        0,
        session_name,
        &buffer.properties,
        EVENT_TRACE_CONTROL_STOP
    );
}

std::wstring to_lower(std::wstring value) {
    for (auto& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }

    return value;
}

std::wstring dos_path_to_device_path(const std::wstring& dos_path) {
    wchar_t full_path[MAX_PATH] = {};
    GetFullPathNameW(dos_path.c_str(), MAX_PATH, full_path, nullptr);

    if (full_path[0] == L'\0' || full_path[1] != L':') {
        return dos_path;
    }

    wchar_t drive[] = { full_path[0], L':', L'\0' };
    wchar_t device_name[1024] = {};

    if (QueryDosDeviceW(drive, device_name, 1024) == 0) {
        return full_path;
    }

    return std::wstring(device_name) + std::wstring(full_path + 2);
}

bool try_parse_pointer(krabs::parser& parser, const wchar_t* name, uint64_t& out) {
    krabs::pointer ptr;
    if (!parser.try_parse(name, ptr)) {
        return false;
    }

    out = ptr.address;
    return true;
}

int main() {
    const wchar_t* trace_name = L"MyTrace";
    stop_stale_trace_session(trace_name);

    const std::wstring target_folder =
        to_lower(dos_path_to_device_path(L"C:\\Users\\Korisnik\\Desktop\\test"));
    const DWORD target_pid = 0; // 0 = all processes, or set this to cmd/notepad PID.
    std::unordered_map<uint64_t, std::wstring> file_object_to_path;

    krabs::user_trace trace(trace_name);
    krabs::provider<> kernelFileProvider(L"Microsoft-Windows-Kernel-File");
    kernelFileProvider.any(0x10 | 0x20 | 0x80 | 0x100 | 0x200 | 0x400 | 0x800 | 0x1000);

    auto file_callback = [&target_folder, &target_pid, &file_object_to_path](
        const EVENT_RECORD& record,
        const krabs::trace_context& trace_context
    ) {
        try {
            //retrieving event specific data
            krabs::schema schema(record, trace_context.schema_locator);
            if (target_pid != 0 && record.EventHeader.ProcessId != target_pid) {
                return;
            }

            if (
                schema.event_id() != 10 &&
                schema.event_id() != 11 &&
                schema.event_id() != 12 &&
                schema.event_id() != 15 &&
                schema.event_id() != 16 &&
                schema.event_id() != 17 &&
                schema.event_id() != 18 &&
                schema.event_id() != 26 &&
                schema.event_id() != 27 &&
                schema.event_id() != 30) {
                return;
            }

            krabs::parser parser(schema);
            std::wstring filePath;
            uint64_t fileObject = 0;
            uint64_t fileKey = 0;

            try_parse_pointer(parser, L"FileObject", fileObject);
            try_parse_pointer(parser, L"FileKey", fileKey);

            parser.try_parse(L"FileName", filePath);
            if (filePath.empty()) {
                parser.try_parse(L"OpenPath", filePath);
            }
            if (filePath.empty()) {
                parser.try_parse(L"FilePath", filePath);
            }
            if (filePath.empty()) {
                parser.try_parse(L"Path", filePath);
            }

            if (filePath.empty() && fileObject != 0) {
                auto it = file_object_to_path.find(fileObject);
                if (it != file_object_to_path.end()) {
                    filePath = it->second;
                }
            }

            if (filePath.empty() && fileKey != 0) {
                auto it = file_object_to_path.find(fileKey);
                if (it != file_object_to_path.end()) {
                    filePath = it->second;
                }
            }

            if (filePath.empty()) {
                return;
            }

            if (to_lower(filePath).find(target_folder) != 0) {
                return;
            }

            if (fileObject != 0) {
                file_object_to_path[fileObject] = filePath;
            }
            if (fileKey != 0) {
                file_object_to_path[fileKey] = filePath;
            }

            std::wcout << L"File event id=" << schema.event_id()
                       << L" task=" << schema.task_name()
                       << L" pid=" << record.EventHeader.ProcessId
                       << L" tid=" << record.EventHeader.ThreadId;

            if (schema.event_id() == 15 || schema.event_id() == 16) {
                uint64_t byteOffset = 0;
                uint32_t ioSize = 0;
                uint32_t ioFlags = 0;

                parser.try_parse(L"ByteOffset", byteOffset);
                parser.try_parse(L"IOSize", ioSize);
                if (ioSize == 0) {
                    parser.try_parse(L"IoSize", ioSize);
                }
                parser.try_parse(L"IOFlags", ioFlags);
                if (ioFlags == 0) {
                    parser.try_parse(L"IoFlags", ioFlags);
                }

                std::wcout << L" offset=" << byteOffset
                           << L" size=" << ioSize
                           << L" flags=0x" << std::hex << ioFlags << std::dec;
            }

            std::wcout << L" path=" << filePath << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "Callback error: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Callback error: unknown exception" << std::endl;
        }

    };

    kernelFileProvider.add_on_event_callback(file_callback);
    trace.enable(kernelFileProvider);

    std::exception_ptr trace_exception = nullptr;

    std::thread traceThread([&trace, &trace_exception]() {
        try {
            trace.start();
        } catch (...) {
            trace_exception = std::current_exception();
        }
    });

    std::wcout << L"Trace radi. Pratim folder: " << target_folder << std::endl;
    if (target_pid != 0) {
        std::wcout << L"Pratim samo PID=" << target_pid << std::endl;
    }
    std::wcout << L"Obrisi neki fajl u tom folderu, pa pritisni Enter za stop." << std::endl;
    std::wcin.get();

    trace.stop();
    traceThread.join();

    if (trace_exception) {
        try {
            std::rethrow_exception(trace_exception);
        } catch (const std::exception& ex) {
            std::cerr << "Trace error: " << ex.what() << std::endl;
            return 1;
        } catch (...) {
            std::cerr << "Trace error: unknown exception" << std::endl;
            return 1;
        }
    }

    return 0;
}
