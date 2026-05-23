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
    kernelFileProvider.any(
        0x80  | // Create/Open
        0x400 | // DeletePath
        0x800 | // RenamePath / SetLinkPath
        0x1000  // CreateNewFile
    );
    uint32_t counter = 0;
    auto file_callback = [&managedProcess, &file_object_to_path, &counter](
        const EVENT_RECORD& record,
        const krabs::trace_context& trace_context
    ) {
        try {
            krabs::schema schema(record, trace_context.schema_locator);
            krabs::parser parser(schema);

            if (!IsProcessInJob(managedProcess.job, record.EventHeader.ProcessId)) {
                return;
            }

            const std::wstring targetPath =
                NormalizeFilePath(L"C:\\Users\\Korisnik\\Desktop\\test");

            auto IsTargetPath = [&](const std::wstring& path) -> bool {
                if (path.empty()) {
                    return false;
                }

                std::wstring normalizedPath = NormalizeFilePath(path);
                return _wcsicmp(normalizedPath.c_str(), targetPath.c_str()) == 0;
            };

            auto PrintAccess = [&](const wchar_t* eventName, const std::wstring& path) {
                std::wcout << L"\n[" << eventName << L"] " << counter++ << '\n';
                std::wcout << L"Process Id: " << record.EventHeader.ProcessId << L"\n";
                std::wcout << L"Path: " << NormalizeFilePath(path) << L"\n";
            };

            auto PrintRename = [&](
                const std::wstring& oldPath,
                const std::wstring& newPath
            ) {
                std::wcout << L"\n[RENAME]\n";
                std::wcout << counter++ << L"\n";
                std::wcout << L"Process Id: " << record.EventHeader.ProcessId << L"\n";

                if (!oldPath.empty()) {
                    std::wcout << L"Old path: " << NormalizeFilePath(oldPath) << L"\n";
                } else {
                    std::wcout << L"Old path: <unknown>\n";
                }

                if (!newPath.empty()) {
                    std::wcout << L"New path: " << NormalizeFilePath(newPath) << L"\n";
                } else {
                    std::wcout << L"New path: <unknown>\n";
                }
            };

            // 12 = Create/Open
            if (schema.event_id() == 12) {
                std::wstring filePath;
                uint64_t fileObject = 0;
                uint32_t raw = 0;

                parser.try_parse(L"FileName", filePath);
                parser.try_parse(L"CreateOptions", raw);
                try_parse_pointer(parser, L"FileObject", fileObject);

                
                if (fileObject != 0 && !filePath.empty()) {
                    file_object_to_path[fileObject] = NormalizeFilePath(filePath);
                }
                
                uint32_t createOptions = raw & 0x00FFFFFF;
                if (!createOptions && IsTargetPath(filePath)) {
                    PrintAccess(L"CREATE/OPEN test.txt", filePath);
                }

                return;
            }

            // 27 = RenamePath
            // Ovo je najkorisniji rename event, jer uglavnom ima FilePath.
            if (schema.event_id() == 27) {
                std::wstring newPath;
                uint64_t fileObject = 0;

                parser.try_parse(L"FilePath", newPath);
                try_parse_pointer(parser, L"FileObject", fileObject);

                std::wstring oldPath;

                auto it = file_object_to_path.find(fileObject);
                if (it != file_object_to_path.end()) {
                    oldPath = it->second;
                }

                bool renamedFromTestTxt = IsTargetPath(oldPath);
                bool renamedToTestTxt = IsTargetPath(newPath);

                if (renamedFromTestTxt || renamedToTestTxt) {
                    PrintRename(oldPath, newPath);
                }

                // Posle rename-a, FileObject sada treba da pokazuje na novu putanju.
                if (fileObject != 0 && !newPath.empty()) {
                    file_object_to_path[fileObject] = NormalizeFilePath(newPath);
                }

                return;
            }

            // 19 = Rename
            // Ovaj event ne mora da ima novu putanju, ali može da kaže da je FileObject
            // koji već znamo bio rename-ovan.
            if (schema.event_id() == 19) {
                uint64_t fileObject = 0;
                try_parse_pointer(parser, L"FileObject", fileObject);

                auto it = file_object_to_path.find(fileObject);
                if (it != file_object_to_path.end()) {
                    const std::wstring& oldPath = it->second;

                    if (IsTargetPath(oldPath)) {
                        PrintRename(oldPath, L"");
                    }
                }

                return;
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
