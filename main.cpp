#include "HcsSandbox/HcsSandbox.h"
#include "StartProcess/StartProcess.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "krabs/krabs.hpp"

#include <iostream>
#include <thread>
#include <exception>
#include <chrono>
#include <atomic>
#include <string>


void StopStaleTraceSession(const wchar_t* session_name) {
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

int main() {
    const wchar_t* trace_name = L"MyTrace";
    StopStaleTraceSession(trace_name);
    std::atomic_bool sandboxReexecutionRequested = false;

    std::wstring codexHomeHost = HcsSandbox::ResolveCodexHomeHost();
    if (!HcsSandbox::DirectoryExists(codexHomeHost)) {
        std::wcerr << L"Namenski Codex home nije pronadjen: "
                   << codexHomeHost << std::endl;
        std::wcerr << L"Kreiraj ga pomocu HCS\\scripts\\Initialize-CodexDirectories.ps1 "
                   << L"ili postavi HCS_CODEX_HOME_HOST." << std::endl;
        return 1;
    }

    FILETIME sessionStartedAfter = {};
    GetSystemTimeAsFileTime(&sessionStartedAfter);

    ManagedJobProcess managedProcess = {};
    if (!StartCmdSuspendedInJob(managedProcess, codexHomeHost)) {
        return 1;
    }

    krabs::user_trace trace(trace_name);
    krabs::provider<> kernelFileProvider(L"Microsoft-Windows-Kernel-File");
    kernelFileProvider.any(
        0x20  | // File I/O
        0x40  | // OperationEnd (correlates Create/Open status)
        0x80  | // Create/Open
        0x400 | // DeletePath
        0x800 | // RenamePath / SetLinkPath
        0x1000  // CreateNewFile
    );
    
    auto eventManager = [&managedProcess, &sandboxReexecutionRequested](
        const EVENT_RECORD& record,
        const krabs::trace_context& trace_context
    ) {
        try {
            krabs::schema schema(record, trace_context.schema_locator);
            krabs::parser parser(schema);

            uint32_t processId = (record.EventHeader.ProcessId);
            bool isValid = true;
            if (schema.event_id() == 24) {
                // OperationEnd can be delivered with a provider/system PID.
                // The handler only accepts IRPs previously captured from this job.
                isValid = OperationEndHandler(parser);
            } else {
                if (!IsProcessInJob(managedProcess.job, processId)) {
                    return;
                }

                if (schema.event_id() == 12) {
                    isValid = CreateOpenHandler(parser, processId);
                } else if (schema.event_id() == 30) {
                    isValid = CreateNewFileHandler(parser, processId);
                } else if (schema.event_id() == 27) {
                    isValid = RenamePathHandler(parser, processId);
                } else if (schema.event_id() == 19) {
                    isValid = RenameHandler(parser, processId);
                }
            }

            if (!isValid && !sandboxReexecutionRequested.exchange(true)) {
                std::wcout << L"[ALERT] Proces pristupa folderu van radnog direktorijuma!" << std::endl;
                std::wcout << L"Gasim trenutni JobObject i prelazim na HCS sandbox." << std::endl;
                TerminateManagedJob(managedProcess, 1);
            }

        } catch (const std::exception& ex) {
            std::cerr << "Callback error: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Callback error: unknown exception" << std::endl;
        }
    };

    kernelFileProvider.add_on_event_callback(eventManager);
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

    if (sandboxReexecutionRequested.load()) {
        std::wstring sessionId = HcsSandbox::FindInterruptedSessionId(
            codexHomeHost,
            sessionStartedAfter
        );
        if (sessionId.empty()) {
            std::wcerr << L"Nije pronadjena Codex sesija pokrenuta u ovom "
                       << L"kontrolisanom procesu; HCS nije startovan." << std::endl;
            return 1;
        }
        return HcsSandbox::StartHcsSandboxReexecution(
            workingDir,
            codexHomeHost,
            sessionId
        );
    }

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
