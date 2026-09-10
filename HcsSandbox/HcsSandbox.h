#pragma once

#include <windows.h>
#include <string>

namespace HcsSandbox {

std::wstring ResolveCodexHomeHost();
bool DirectoryExists(const std::wstring& path);
std::wstring FindInterruptedSessionId(
    const std::wstring& codexHome,
    const FILETIME& sessionStartedAfter
);
int StartHcsSandboxReexecution(
    const std::wstring& allowedWorkingDir,
    const std::wstring& codexHomeHost,
    const std::wstring& sessionId
);

} // namespace HcsSandbox
