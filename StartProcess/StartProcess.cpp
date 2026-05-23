#include "StartProcess.h"

#include <exception>
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace {
    std::mutex g_knownJobPidsMutex;
    std::unordered_set<DWORD> g_knownJobPids;

    DWORD JobMessageProcessId(LPOVERLAPPED overlapped) {
        return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(overlapped));
    }

    void RememberJobPid(DWORD processId) {
        if (processId == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        g_knownJobPids.insert(processId);
    }

    void RememberJobPids(const std::vector<DWORD>& processIds) {
        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        for (DWORD processId : processIds) {
            if (processId != 0) {
                g_knownJobPids.insert(processId);
            }
        }
    }

    bool IsKnownJobPid(DWORD processId) {
        std::lock_guard<std::mutex> lock(g_knownJobPidsMutex);
        return g_knownJobPids.find(processId) != g_knownJobPids.end();
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
            RememberJobPid(messageProcessId);
            std::wcout << L"[JOB] New process: " << messageProcessId << L"\n";
            PrintProcessesInJob(managedProcess.job);
            continue;
        }

        if (message == JOB_OBJECT_MSG_EXIT_PROCESS ||
            message == JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS) {
            // Nemoj brisati PID iz g_knownJobPids.
            // ETW eventovi za kratkotrajne procese mogu da stignu tek nakon exit-a.
            std::wcout << L"[JOB] Process exited: " << messageProcessId << L"\n";
            PrintProcessesInJob(managedProcess.job);
            continue;
        }

        if (message == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
            return true;
        }
    }
}

std::vector<DWORD> GetProcessIdsInJob(HANDLE job) {
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
            std::cerr << "QueryInformationJobObject failed. GetLastError = " << GetLastError() << '\n';
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

        RememberJobPids(processIds);
        return processIds;
    }
}

bool IsProcessInJob(HANDLE job, DWORD processId) {
    // Prvo proveri cache svih PID-eva koji su ikada bili u job-u.
    // Ovo je bitno za ETW, jer event za kratkotrajan proces moze da stigne
    // nakon sto je proces vec izasao iz JobObject-a.
    if (IsKnownJobPid(processId)) {
        return true;
    }

    std::vector<DWORD> processIds = GetProcessIdsInJob(job);

    for (DWORD jobProcessId : processIds) {
        if (jobProcessId == processId) {
            RememberJobPid(processId);
            return true;
        }
    }

    return false;
}

void PrintProcessesInJob(HANDLE job) {
    return;
    std::vector<DWORD> processIds = GetProcessIdsInJob(job);
    RememberJobPids(processIds);

    std::wcout << L"Processes in JobObject: " << processIds.size() << L"\n";
    for (DWORD processId : processIds) {
        std::wcout << L"  PID: " << processId << L"\n";
    }
}

bool StartCmdSuspendedInJob(ManagedJobProcess& managedProcess) {
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    wchar_t cmdLine[] = L"C:\\Windows\\System32\\cmd.exe";
    wchar_t workingDir[] = L"C:\\Users\\Korisnik\\Desktop\\test";
    if (!CreateProcessW(
            nullptr,
            cmdLine,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
            nullptr,
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
    RememberJobPid(managedProcess.processId);

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
