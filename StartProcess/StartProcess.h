#pragma once

#include <windows.h>

#include <vector>

struct ManagedJobProcess {
    HANDLE job = nullptr;
    HANDLE completionPort = nullptr;
    HANDLE process = nullptr;
    HANDLE mainThread = nullptr;
    HANDLE jobMonitorThread = nullptr;
    DWORD processId = 0;
    DWORD threadId = 0;
};

bool StartCmdSuspendedInJob(ManagedJobProcess& managedProcess);
bool ResumeManagedProcess(ManagedJobProcess& managedProcess);
bool WaitForManagedJobToFinish(ManagedJobProcess& managedProcess);
bool IsProcessInJob(HANDLE job, DWORD processId);
std::vector<DWORD> GetProcessIdsInJob(HANDLE job);
void PrintProcessesInJob(HANDLE job);
void CloseManagedJobProcess(ManagedJobProcess& managedProcess);
