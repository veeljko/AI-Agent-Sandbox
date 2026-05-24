#pragma once

#include <windows.h>

#include <thread>
#include <vector>

struct ManagedJobProcess {
    HANDLE job = nullptr;
    HANDLE completionPort = nullptr;
    HANDLE process = nullptr;
    HANDLE mainThread = nullptr;
    std::thread jobMonitorThread;
    bool jobMonitorSucceeded = false;
    DWORD processId = 0;
    DWORD threadId = 0;
};
extern const wchar_t workingDir[];
bool StartCmdSuspendedInJob(ManagedJobProcess& managedProcess);
bool ResumeManagedProcess(ManagedJobProcess& managedProcess);
bool WaitForManagedJobToFinish(ManagedJobProcess& managedProcess);
bool IsProcessInJob(HANDLE job, DWORD processId);
std::vector<DWORD> GetProcessIdsInJob(HANDLE job);
void PrintProcessesInJob(HANDLE job);
void CloseManagedJobProcess(ManagedJobProcess& managedProcess);
