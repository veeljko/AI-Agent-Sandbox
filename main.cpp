#include "StartProcess/StartProcess.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "krabs/krabs.hpp"

#include <iostream>
#include <thread>
#include <exception>
#include <chrono>
#include <atomic>
#include <string>
#include <utility>
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

bool DirectoryExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool EnsureDirectoryExists(const std::wstring& path) {
    if (DirectoryExists(path)) {
        return true;
    }
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    DWORD error = GetLastError();
    return error == ERROR_ALREADY_EXISTS && DirectoryExists(path);
}

bool InstallHcsCodexSupportFiles(
    const std::wstring& exeDir,
    const std::wstring& codexHomeHost
) {
    const std::wstring toolsDirectory = codexHomeHost + L"\\hcs-tools";
    const std::wstring rulesDirectory = codexHomeHost + L"\\rules";
    if (!EnsureDirectoryExists(toolsDirectory) ||
        !EnsureDirectoryExists(rulesDirectory)) {
        std::wcerr << L"Ne mogu da kreiram HCS Codex support direktorijume. "
                   << L"GetLastError = " << GetLastError() << std::endl;
        return false;
    }

    const std::vector<std::pair<std::wstring, std::wstring>> files = {
        {
            exeDir + L"\\HCS\\workspace-delete.exe",
            toolsDirectory + L"\\workspace-delete.exe"
        },
        {
            exeDir + L"\\HCS\\rules\\hcs-workspace.rules",
            rulesDirectory + L"\\hcs-workspace.rules"
        },
        {
            exeDir + L"\\HCS\\hcs-workspace.config.toml",
            codexHomeHost + L"\\hcs-workspace.config.toml"
        }
    };

    for (const auto& file : files) {
        if (!PathExists(file.first)) {
            std::wcerr << L"Nedostaje HCS Codex support fajl: "
                       << file.first << std::endl;
            return false;
        }
        if (!CopyFileW(file.first.c_str(), file.second.c_str(), FALSE)) {
            std::wcerr << L"Ne mogu da instaliram HCS Codex support fajl "
                       << file.second << L". GetLastError = "
                       << GetLastError() << std::endl;
            return false;
        }
    }
    return true;
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

std::wstring ResolveCodexHomeHost() {
    std::wstring codexHome = GetEnvironmentValue(L"HCS_CODEX_HOME_HOST");
    if (codexHome.empty()) {
        codexHome = GetEnvironmentValue(L"CODEX_HOME");
    }
    if (codexHome.empty()) {
        std::wstring userProfile = GetEnvironmentValue(L"USERPROFILE");
        if (!userProfile.empty()) {
            codexHome = userProfile + L"\\.codex-hcs";
        }
    }
    return codexHome;
}

bool IsHexCharacter(wchar_t value) {
    return (value >= L'0' && value <= L'9') ||
           (value >= L'a' && value <= L'f') ||
           (value >= L'A' && value <= L'F');
}

std::wstring ExtractSessionId(const std::wstring& fileName) {
    const std::wstring prefix = L"rollout-";
    const std::wstring suffix = L".jsonl";
    constexpr size_t uuidLength = 36;

    if (fileName.size() < prefix.size() + uuidLength + suffix.size() ||
        fileName.compare(0, prefix.size(), prefix) != 0 ||
        fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return L"";
    }

    size_t uuidStart = fileName.size() - suffix.size() - uuidLength;
    std::wstring sessionId = fileName.substr(uuidStart, uuidLength);
    for (size_t index = 0; index < sessionId.size(); ++index) {
        bool isHyphen =
            index == 8 || index == 13 || index == 18 || index == 23;
        if ((isHyphen && sessionId[index] != L'-') ||
            (!isHyphen && !IsHexCharacter(sessionId[index]))) {
            return L"";
        }
    }
    return sessionId;
}

struct SessionCandidate {
    FILETIME lastWriteTime = {};
    std::wstring id;
};

void FindNewestSession(
    const std::wstring& directory,
    const FILETIME& notBefore,
    SessionCandidate& candidate
) {
    std::wstring searchPath = directory;
    if (!searchPath.empty() &&
        searchPath.back() != L'\\' &&
        searchPath.back() != L'/') {
        searchPath += L'\\';
    }
    searchPath += L'*';

    WIN32_FIND_DATAW findData = {};
    HANDLE search = FindFirstFileW(searchPath.c_str(), &findData);
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        std::wstring fullPath = directory;
        if (!fullPath.empty() &&
            fullPath.back() != L'\\' &&
            fullPath.back() != L'/') {
            fullPath += L'\\';
        }
        fullPath += name;

        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                FindNewestSession(fullPath, notBefore, candidate);
            }
            continue;
        }

        std::wstring sessionId = ExtractSessionId(name);
        if (sessionId.empty() ||
            CompareFileTime(&findData.ftLastWriteTime, &notBefore) < 0) {
            continue;
        }

        if (candidate.id.empty() ||
            CompareFileTime(
                &findData.ftLastWriteTime,
                &candidate.lastWriteTime
            ) > 0) {
            candidate.lastWriteTime = findData.ftLastWriteTime;
            candidate.id = std::move(sessionId);
        }
    } while (FindNextFileW(search, &findData));

    FindClose(search);
}

std::wstring FindInterruptedSessionId(
    const std::wstring& codexHome,
    const FILETIME& sessionStartedAfter
) {
    SessionCandidate candidate;
    FindNewestSession(
        codexHome + L"\\sessions",
        sessionStartedAfter,
        candidate
    );
    return candidate.id;
}

int StartHcsSandboxReexecution(
    const std::wstring& allowedWorkingDir,
    const std::wstring& codexHomeHost,
    const std::wstring& sessionId
) {
    std::wstring exeDir = GetExecutableDirectory();
    std::wstring runnerPath = GetEnvironmentValue(L"HCS_SANDBOX_RUNNER");
    if (runnerPath.empty()) {
        runnerPath = exeDir + L"\\HCS\\sandbox-runner.exe";
    }

    if (!PathExists(runnerPath)) {
        std::wcerr << L"Novi HCS sandbox runner nije pronadjen: " << runnerPath << std::endl;
        std::wcerr << L"Build: cd HCS && go build -buildvcs=false -o sandbox-runner.exe .\\cmd\\sandbox-runner" << std::endl;
        return 1;
    }

    if (!DirectoryExists(codexHomeHost)) {
        std::wcerr << L"Codex home za HCS nije pronadjen: " << codexHomeHost << std::endl;
        std::wcerr << L"Postavi HCS_CODEX_HOME_HOST na pripremljen folder koji sadrzi Codex auth/config/session stanje." << std::endl;
        return 1;
    }
    if (!InstallHcsCodexSupportFiles(exeDir, codexHomeHost)) {
        return 1;
    }

    const std::wstring workspaceContainer = L"C:\\WorkingDirectory";
    const std::wstring resumeCommand =
        L"codex.cmd resume --dangerously-bypass-approvals-and-sandbox"
        L" -p hcs-workspace"
        L" --cd " + QuoteArgument(workspaceContainer) + L" " + sessionId;
    std::wstring commandLine =
        QuoteArgument(runnerPath) +
        L" -workspace " + QuoteArgument(allowedWorkingDir) +
        L" -workspace-container " + QuoteArgument(workspaceContainer) +
        L" -workdir " + QuoteArgument(workspaceContainer) +
        L" -codex-home " + QuoteArgument(codexHomeHost) +
        L" -codex-home-container " + QuoteArgument(codexHomeHost) +
        L" -tty" +
        L" -command-line " + QuoteArgument(resumeCommand);

    std::wcout << L"Nastavljam prekinutu Codex sesiju u novom HCS sandbox-u..." << std::endl;
    std::wcout << L"Session ID: " << sessionId << std::endl;
    std::wcout << L"Host mountovi: " << allowedWorkingDir
               << L" -> " << workspaceContainer << L", " << codexHomeHost
               << L" -> " << codexHomeHost << std::endl;
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

    std::wstring codexHomeHost = ResolveCodexHomeHost();
    if (!DirectoryExists(codexHomeHost)) {
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
        std::wstring sessionId = FindInterruptedSessionId(
            codexHomeHost,
            sessionStartedAfter
        );
        if (sessionId.empty()) {
            std::wcerr << L"Nije pronadjena Codex sesija pokrenuta u ovom "
                       << L"kontrolisanom procesu; HCS nije startovan." << std::endl;
            return 1;
        }
        return StartHcsSandboxReexecution(
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
