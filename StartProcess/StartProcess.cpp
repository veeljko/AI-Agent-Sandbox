#include "StartProcess.h"

#include <algorithm>
#include <cwchar>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

const wchar_t workingDir[] = L"C:\\Users\\Korisnik\\Desktop\\test";

namespace {
    std::mutex g_knownJobPidsMutex;
    std::unordered_map<HANDLE, std::unordered_set<DWORD>> g_knownJobPids;

    DWORD JobMessageProcessId(LPOVERLAPPED overlapped) {
        return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(overlapped));
    }

    void RememberJobPid(HANDLE job, DWORD processId) {
        if (processId == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        g_knownJobPids[job].insert(processId);
    }

    void RememberJobPids(HANDLE job, const std::vector<DWORD>& processIds) {
        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        for (DWORD processId : processIds) {
            if (processId != 0) {
                g_knownJobPids[job].insert(processId);
            }
        }
    }

    bool IsKnownJobPid(HANDLE job, DWORD processId) {
        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        auto found = g_knownJobPids.find(job);
        return found != g_knownJobPids.end() && found->second.count(processId) != 0;
    }

    std::vector<wchar_t> BuildChildEnvironment(
        const std::wstring& codexHome
    ) {
        std::vector<std::wstring> entries;
        wchar_t* environment = GetEnvironmentStringsW();
        if (environment == nullptr) {
            return {};
        }

        const std::wstring codexHomePrefix = L"CODEX_HOME=";
        for (const wchar_t* current = environment;
             *current != L'\0';
             current += wcslen(current) + 1) {
            std::wstring entry(current);
            bool isCodexHome =
                entry.size() >= codexHomePrefix.size() &&
                _wcsnicmp(
                    entry.c_str(),
                    codexHomePrefix.c_str(),
                    codexHomePrefix.size()
                ) == 0;
            if (!isCodexHome) {
                entries.push_back(std::move(entry));
            }
        }
        FreeEnvironmentStringsW(environment);

        entries.push_back(codexHomePrefix + codexHome);
        std::sort(
            entries.begin(),
            entries.end(),
            [](const std::wstring& left, const std::wstring& right) {
                return _wcsicmp(left.c_str(), right.c_str()) < 0;
            }
        );

        size_t required = 1;
        for (const std::wstring& entry : entries) {
            required += entry.size() + 1;
        }

        std::vector<wchar_t> block;
        block.reserve(required);
        for (const std::wstring& entry : entries) {
            block.insert(block.end(), entry.begin(), entry.end());
            block.push_back(L'\0');
        }
        block.push_back(L'\0');
        return block;
    }
}

bool MonitorJob(ManagedJobProcess& managedProcess) {
    for (;;) {
        DWORD message = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            managedProcess.completionPort,
            &message,
            &completionKey,
            &overlapped,
            INFINITE);

        if (!ok) {
            std::cerr << "GetQueuedCompletionStatus failed. GetLastError = " << GetLastError() << '\n';
            return false;
        }

        if (reinterpret_cast<HANDLE>(completionKey) != managedProcess.job) {
            continue;
        }

        DWORD messageProcessId = JobMessageProcessId(overlapped);

        if (message == JOB_OBJECT_MSG_NEW_PROCESS) {
            RememberJobPid(managedProcess.job, messageProcessId);
            // Process lifecycle is reported once by KernelProcessProvider.
            // PrintProcessesInJob(managedProcess.job);
            continue;
        }

        if (message == JOB_OBJECT_MSG_EXIT_PROCESS ||
            message == JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS) {
            // Nemoj brisati PID iz g_knownJobPids.
            // ETW eventovi za kratkotrajne procese mogu da stignu tek nakon exit-a.
            // std::wcout << L"[JOB] Process exited: " << messageProcessId << L"\n";
            // PrintProcessesInJob(managedProcess.job);
            continue;
        }

        if (message == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
            return true;
        }
    }
}

std::vector<DWORD> GetProcessIdsInJob(HANDLE job) {
    if (!job || job == INVALID_HANDLE_VALUE) return {};
    DWORD maxProcesses = 16;

    for (;;) {
        DWORD bufferSize =
            sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) +
            sizeof(ULONG_PTR) * maxProcesses;
        std::vector<BYTE> buffer(bufferSize);

        auto processList =
            reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());

        if (!QueryInformationJobObject(
                job,
                JobObjectBasicProcessIdList,
                processList,
                bufferSize,
                nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_MORE_DATA) {
                maxProcesses = (std::max)(maxProcesses * 2, processList->NumberOfAssignedProcesses + 8);
                continue;
            }
            std::cerr << "QueryInformationJobObject failed. GetLastError = " << error << '\n';
            return {};
        }

        if (processList->NumberOfAssignedProcesses > processList->NumberOfProcessIdsInList) {
            maxProcesses = processList->NumberOfAssignedProcesses + 8;
            continue;
        }

        std::vector<DWORD> processIds;
        processIds.reserve(processList->NumberOfProcessIdsInList);

        for (DWORD i = 0; i < processList->NumberOfProcessIdsInList; i++) {
            processIds.push_back(static_cast<DWORD>(processList->ProcessIdList[i]));
        }

        RememberJobPids(job, processIds);
        return processIds;
    }
}

bool IsProcessInJob(HANDLE job, DWORD processId) {
    if (!job || job == INVALID_HANDLE_VALUE || !processId) return false;
    // Validate live PIDs against the actual job. A recycled PID must not be
    // accepted solely because a previous process with that PID belonged to it.
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process) {
        BOOL inJob = FALSE;
        const BOOL queried = ::IsProcessInJob(process, job, &inJob);
        CloseHandle(process);
        if (queried) {
            if (inJob) RememberJobPid(job, processId);
            return inJob != FALSE;
        }
    } else if (GetLastError() == ERROR_INVALID_PARAMETER && IsKnownJobPid(job, processId)) {
        // Short-lived process already exited; retain it for delayed ETW events.
        return true;
    }
    const auto processIds = GetProcessIdsInJob(job);
    return std::find(processIds.begin(), processIds.end(), processId) != processIds.end();
}

void PrintProcessesInJob(HANDLE job) {
    return;
    std::vector<DWORD> processIds = GetProcessIdsInJob(job);
    RememberJobPids(job, processIds);

    std::wcout << L"Processes in JobObject: " << processIds.size() << L"\n";
    for (DWORD processId : processIds) {
        std::wcout << L"  PID: " << processId << L"\n";
    }
}

bool StartCmdSuspendedInJob(
    ManagedJobProcess& managedProcess,
    const std::wstring& codexHome
) {
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<wchar_t> environment = BuildChildEnvironment(codexHome);
    if (environment.empty()) {
        std::cerr << "GetEnvironmentStringsW failed. GetLastError = "
                  << GetLastError() << '\n';
        return false;
    }

    wchar_t cmdLine[] = L"C:\\Windows\\System32\\cmd.exe";
    const std::wstring originalCommandLine = cmdLine;
    if (!CreateProcessW(
            nullptr,
            cmdLine,
            nullptr,
            nullptr,
            TRUE,
            CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
            environment.data(),
            workingDir,
            &si,
            &pi)) {
        std::cerr << "CreateProcessW failed. GetLastError = " << GetLastError() << '\n';
        return false;
    }

    managedProcess.process = pi.hProcess;
    managedProcess.mainThread = pi.hThread;
    managedProcess.processId = pi.dwProcessId;
    managedProcess.threadId = pi.dwThreadId;
    managedProcess.commandLine = originalCommandLine;

    managedProcess.job = CreateJobObjectW(nullptr, nullptr);
    if (managedProcess.job == nullptr) {
        std::cerr << "CreateJobObjectW failed. GetLastError = " << GetLastError() << '\n';
        TerminateProcess(managedProcess.process, 1);
        CloseManagedJobProcess(managedProcess);
        return false;
    }

    managedProcess.completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (managedProcess.completionPort == nullptr) {
        std::cerr << "CreateIoCompletionPort failed. GetLastError = " << GetLastError() << '\n';
        TerminateProcess(managedProcess.process, 1);
        CloseManagedJobProcess(managedProcess);
        return false;
    }

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT completionPortInfo = {};
    completionPortInfo.CompletionKey = managedProcess.job;
    completionPortInfo.CompletionPort = managedProcess.completionPort;

    if (!SetInformationJobObject(
            managedProcess.job,
            JobObjectAssociateCompletionPortInformation,
            &completionPortInfo,
            sizeof(completionPortInfo))) {
        std::cerr << "SetInformationJobObject failed. GetLastError = " << GetLastError() << '\n';
        TerminateProcess(managedProcess.process, 1);
        CloseManagedJobProcess(managedProcess);
        return false;
    }

    if (!AssignProcessToJobObject(managedProcess.job, managedProcess.process)) {
        std::cerr << "AssignProcessToJobObject failed. GetLastError = " << GetLastError() << '\n';
        TerminateProcess(managedProcess.process, 1);
        CloseManagedJobProcess(managedProcess);
        return false;
    }

    // Obavezno odmah zapamti glavni PID. Nemoj se oslanjati samo na completion-port poruku.
    RememberJobPid(managedProcess.job, managedProcess.processId);

    try {
        managedProcess.jobMonitorSucceeded = false;
        managedProcess.jobMonitorThread = std::thread([&managedProcess]() {
            managedProcess.jobMonitorSucceeded = MonitorJob(managedProcess);
        });
    } catch (const std::exception& ex) {
        std::cerr << "std::thread for JobMonitor failed: " << ex.what() << '\n';
        TerminateProcess(managedProcess.process, 1);
        CloseManagedJobProcess(managedProcess);
        return false;
    }

    std::wcout << L"Started suspended cmd.exe\n";
    std::wcout << L"PID: " << managedProcess.processId << L"\n";
    std::wcout << L"TID: " << managedProcess.threadId << L"\n";
    std::wcout << L"Assigned process to JobObject\n";
    PrintProcessesInJob(managedProcess.job);

    return true;
}

bool ResumeManagedProcess(ManagedJobProcess& managedProcess) {
    DWORD resumeResult = 0;
    DWORD error = ERROR_SUCCESS;

    try {
        std::thread resumeWorker([&managedProcess, &resumeResult, &error]() {
            resumeResult = ResumeThread(managedProcess.mainThread);
            if (resumeResult == static_cast<DWORD>(-1)) {
                error = GetLastError();
                return;
            }

            error = ERROR_SUCCESS;
        });
        resumeWorker.join();
    } catch (const std::exception& ex) {
        std::cerr << "std::thread failed: " << ex.what() << '\n';
        return false;
    }

    if (resumeResult == static_cast<DWORD>(-1)) {
        std::cerr << "ResumeThread failed. GetLastError = " << error << '\n';
        return false;
    }

    std::wcout << L"Resumed cmd.exe from worker thread\n";
    PrintProcessesInJob(managedProcess.job);
    return true;
}

bool WaitForManagedJobToFinish(ManagedJobProcess& managedProcess) {
    if (!managedProcess.jobMonitorThread.joinable()) {
        return false;
    }

    managedProcess.jobMonitorThread.join();

    if (!managedProcess.jobMonitorSucceeded) {
        return false;
    }

    std::wcout << L"JobObject has no running processes\n";
    PrintProcessesInJob(managedProcess.job);
    return true;
}

void TerminateManagedJob(ManagedJobProcess& managedProcess, UINT exitCode) {
    if (managedProcess.job != nullptr) {
        TerminateJobObject(managedProcess.job, exitCode);
        return;
    }

    if (managedProcess.process != nullptr) {
        TerminateProcess(managedProcess.process, exitCode);
    }
}

void CloseManagedJobProcess(ManagedJobProcess& managedProcess) {
    if (managedProcess.jobMonitorThread.joinable()) {
        if (managedProcess.completionPort != nullptr) {
            PostQueuedCompletionStatus(
                managedProcess.completionPort,
                JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO,
                reinterpret_cast<ULONG_PTR>(managedProcess.job),
                nullptr);
        }
        managedProcess.jobMonitorThread.join();
    }

    if (managedProcess.completionPort != nullptr) {
        CloseHandle(managedProcess.completionPort);
        managedProcess.completionPort = nullptr;
    }

    if (managedProcess.job != nullptr) {
        {
            std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
            g_knownJobPids.erase(managedProcess.job);
        }
        CloseHandle(managedProcess.job);
        managedProcess.job = nullptr;
    }

    if (managedProcess.mainThread != nullptr) {
        CloseHandle(managedProcess.mainThread);
        managedProcess.mainThread = nullptr;
    }

    if (managedProcess.process != nullptr) {
        CloseHandle(managedProcess.process);
        managedProcess.process = nullptr;
    }

    managedProcess.processId = 0;
    managedProcess.threadId = 0;
}
