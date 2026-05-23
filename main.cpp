#include "StartProcess/StartProcess.h"
#include "NormalizePath/NormalizePath.h"
#include "FilterFiles/FilterFiles.h"
#include "krabs/krabs.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <exception>
#include <chrono>
#include <cwctype>
#include <map>


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
    try {
        krabs::pointer ptr;
        if (parser.try_parse(name, ptr)) {
            out = ptr.address;
            return true;
        }
    } catch (...) {
    }

    try {
        uint64_t value = 0;
        if (parser.try_parse(name, value)) {
            out = value;
            return true;
        }
    } catch (...) {
    }

    try {
        uint32_t value = 0;
        if (parser.try_parse(name, value)) {
            out = value;
            return true;
        }
    } catch (...) {
    }

    return false;
}

int main() {
    const wchar_t* trace_name = L"MyTrace";
    stop_stale_trace_session(trace_name);

    ManagedJobProcess managedProcess = {};
    if (!StartCmdSuspendedInJob(managedProcess)) {
        return 1;
    }

    std::map<uint64_t, std::wstring> file_object_to_path;

    krabs::user_trace trace(trace_name);
    krabs::provider<> kernelFileProvider(L"Microsoft-Windows-Kernel-File");
    kernelFileProvider.any(0x10 | 0x20 | 0x80 | 0x100 | 0x200 | 0x400 | 0x800 | 0x1000);
    uint32_t counter = 0;
    auto file_callback = [&managedProcess, &file_object_to_path, &counter](
        const EVENT_RECORD& record,
        const krabs::trace_context& trace_context
    ) {
        try {
            //retrieving event specific data
            krabs::schema schema(record, trace_context.schema_locator);

            if (!IsProcessInJob(managedProcess.job, record.EventHeader.ProcessId)) {
                return;
            }

            if (schema.event_id() == 12) {
                krabs::parser parser(schema);

                std::wstring filePath;
                uint64_t fileObject = 0;
                uint32_t raw = 0;

                try_parse_pointer(parser, L"FileObject", fileObject);
                parser.try_parse(L"FileName", filePath);
                parser.try_parse(L"CreateOptions", raw);

                uint32_t createOptions = raw & 0x00FFFFFF;
                uint32_t createDisposition = (raw >> 24) & 0xFF;

                std::wstring normalizedPath = NormalizeFilePath(filePath);

                if (createOptions & 0x00000001) {
                    return;
                    std::wcout << "DIREKTORIJUM\n";
                }

                if (createOptions & 0x00000040) {
                    std::wstring normalizedPath = NormalizeFilePath(filePath);
                    std::wstring targetPath = NormalizeFilePath(L"C:\\Users\\Korisnik\\Desktop\\test\\test.txt");
                    if (_wcsicmp(normalizedPath.c_str(), targetPath.c_str()) == 0) {
                        std :: wcout << counter++ <<'\n';
                        std::wcout << "Process Id: \n" << record.EventHeader.ProcessId << '\n';
                        std::wcout << "Path:\n" << normalizedPath << '\n';
                    }
                }
            }
            
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

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (trace_exception) {
        traceThread.join();
        TerminateProcess(managedProcess.process, 1);
        WaitForManagedJobToFinish(managedProcess);
        CloseManagedJobProcess(managedProcess);

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

    std::wcout << L"Trace radi. Pratim procese iz JobObject-a." << std::endl;

    if (!ResumeManagedProcess(managedProcess)) {
        trace.stop();
        traceThread.join();
        TerminateProcess(managedProcess.process, 1);
        WaitForManagedJobToFinish(managedProcess);
        CloseManagedJobProcess(managedProcess);
        return 1;
    }

    std::wcout << L"Zatvori cmd.exe da se program zavrsi." << std::endl;
    WaitForManagedJobToFinish(managedProcess);

    trace.stop();
    traceThread.join();
    CloseManagedJobProcess(managedProcess);

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
