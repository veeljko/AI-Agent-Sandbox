#include "StartProcess/StartProcess.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "krabs/krabs.hpp"

#include <iostream>
#include <thread>
#include <exception>
#include <chrono>
#include <atomic>
#include <string>
#include <vector>


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

std::wstring GetEnvironmentValue(const wchar_t* name) {
    DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(required);
    DWORD written = GetEnvironmentVariableW(
        name,
        buffer.data(),
        static_cast<DWORD>(buffer.size())
    );

    if (written == 0 || written >= buffer.size()) {
        return L"";
    }

    return std::wstring(buffer.data(), written);
}

std::wstring GetExecutableDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        DWORD written = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );

        if (written == 0) {
            return L".";
        }

        if (written < buffer.size() - 1) {
            std::wstring exePath(buffer.data(), written);
            size_t slash = exePath.find_last_of(L"\\/");
            if (slash == std::wstring::npos) {
                return L".";
            }
            return exePath.substr(0, slash);
        }

        buffer.resize(buffer.size() * 2);
    }
}

bool PathExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring QuoteArgument(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }

    if (value.find_first_of(L" \t\"") == std::wstring::npos) {
        return value;
    }

    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L'\\';
        }
        quoted += ch;
    }
    quoted += L"\"";
    return quoted;
}

int StartHcsSandboxReexecution(const std::wstring& allowedWorkingDir) {
    const wchar_t* defaultLayerPath =
        LR"(C:\ProgramData\Docker\windowsfilter\82dfc8b8aee8ab12f3af86fb21fffbc2e1395888b52ac4d069c4b1bca7cf39bc)";

    std::wstring exeDir = GetExecutableDirectory();
    std::wstring runnerPath = GetEnvironmentValue(L"HCS_SANDBOX_RUNNER");
    if (runnerPath.empty()) {
        runnerPath = exeDir + L"\\hcs_sandbox_runner.exe";
    }

    std::wstring layerPath = GetEnvironmentValue(L"HCS_LAYER_PATH");
    if (layerPath.empty()) {
        layerPath = defaultLayerPath;
    }

    std::wstring codexHomeHost = GetEnvironmentValue(L"HCS_CODEX_HOME_HOST");
    if (codexHomeHost.empty()) {
        std::wstring hcsCodexHome = L"C:\\Users\\Korisnik\\.codex-hcs";
        codexHomeHost = PathExists(hcsCodexHome) ? hcsCodexHome : L"C:\\Users\\Korisnik\\.codex";
    }

    if (!PathExists(runnerPath)) {
        std::wcerr << L"HCS sandbox runner nije pronadjen: " << runnerPath << std::endl;
        std::wcerr << L"Build: go build -o ..\\Project3\\hcs_sandbox_runner.exe . iz Go Hello World foldera." << std::endl;
        return 1;
    }

    if (!PathExists(layerPath)) {
        std::wcerr << L"HCS layer path nije pronadjen: " << layerPath << std::endl;
        std::wcerr << L"Postavi HCS_LAYER_PATH na top windowsfilter layer folder." << std::endl;
        return 1;
    }

    if (!PathExists(codexHomeHost)) {
        std::wcerr << L"Codex home za HCS nije pronadjen: " << codexHomeHost << std::endl;
        std::wcerr << L"Postavi HCS_CODEX_HOME_HOST na folder koji sadrzi Codex auth/config." << std::endl;
        return 1;
    }

    std::wstring commandLine =
        QuoteArgument(runnerPath) +
        L" -layer " + QuoteArgument(layerPath) +
        L" -workdir " + QuoteArgument(allowedWorkingDir) +
        L" -mount-ro \"C:\\Program Files\\nodejs=C:\\tools\\nodejs\"" +
        L" -mount-ro \"C:\\Users\\Korisnik\\AppData\\Roaming\\npm=C:\\tools\\npm\"" +
        L" -mount " + QuoteArgument(codexHomeHost + L"=C:\\profile\\.codex") +
        L" -env \"USERPROFILE=C:\\profile\"" +
        L" -env \"HOME=C:\\profile\"" +
        L" -env \"CODEX_HOME=C:\\profile\\.codex\"" +
        L" -env \"PATH=C:\\tools\\nodejs;C:\\tools\\npm;C:\\Windows\\System32;C:\\Windows\"";

    std::wcout << L"Restartujem proces u HCS sandbox-u..." << std::endl;
    std::wcout << L"Komanda: " << commandLine << std::endl;

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    if (!CreateProcessW(
            nullptr,
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            exeDir.c_str(),
            &si,
            &pi)) {
        std::wcerr << L"CreateProcessW za HCS runner nije uspeo. GetLastError = "
                   << GetLastError() << std::endl;
        return 1;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        std::wcerr << L"GetExitCodeProcess za HCS runner nije uspeo. GetLastError = "
                   << GetLastError() << std::endl;
    }

    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}


int main() {
    const wchar_t* trace_name = L"MyTrace";
    StopStaleTraceSession(trace_name);
    std::atomic_bool sandboxReexecutionRequested = false;

    ManagedJobProcess managedProcess = {};
    if (!StartCmdSuspendedInJob(managedProcess)) {
        return 1;
    }

    krabs::user_trace trace(trace_name);
    krabs::provider<> kernelFileProvider(L"Microsoft-Windows-Kernel-File");
    kernelFileProvider.any(
        0x20  | // File I/O
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

            if (!IsProcessInJob(managedProcess.job, record.EventHeader.ProcessId)) {
                return;
            }

            uint32_t processId = (record.EventHeader.ProcessId);
            bool isValid = true;
            if (schema.event_id() == 12) isValid = CreateOpenHandler(parser, processId);
            else if (schema.event_id() == 27) isValid = RenamePathHandler(parser, processId);
            else if (schema.event_id() == 19) isValid = RenameHandler(parser, processId);

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
        return StartHcsSandboxReexecution(workingDir);
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
