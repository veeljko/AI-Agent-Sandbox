#include "ProcessHandlerCommon.h"
#include "../../../FilterFiles/FilterFiles.h"
#include <sddl.h>
#include <iostream>
#include <mutex>

uint64_t FileTimeTicks(const FILETIME& time) {
    return (uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime;
}
TimePoint FromFileTime(const FILETIME& time) {
    const auto ticks = static_cast<int64_t>(FileTimeTicks(time));
    using WindowsDuration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    return TimePoint(std::chrono::duration_cast<TimePoint::duration>(
        WindowsDuration(ticks - 116444736000000000LL)));
}
void EnrichLiveProcess(ProcessContext& process) {
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid);
    if (!handle) return;
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(handle, &created, &exited, &kernel, &user) ||
        (process.createTimeTicks && process.createTimeTicks != FileTimeTicks(created))) {
        CloseHandle(handle);
        return;
    }
    process.createTimeTicks = FileTimeTicks(created);
    process.startTime = FromFileTime(created);
    if (process.image.empty()) {
        std::vector<wchar_t> image(32768);
        DWORD length = static_cast<DWORD>(image.size());
        if (QueryFullProcessImageNameW(handle, 0, image.data(), &length)) {
            process.image.assign(image.data(), length);
        }
    }
    HANDLE token = nullptr;
    if (OpenProcessToken(handle, TOKEN_QUERY, &token)) {
        DWORD required = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &required);
        std::vector<BYTE> buffer(required);
        if (required && GetTokenInformation(token, TokenUser, buffer.data(), required, &required)) {
            auto* tokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
            wchar_t* sid = nullptr;
            if (ConvertSidToStringSidW(tokenUser->User.Sid, &sid)) {
                process.userSid = sid;
                LocalFree(sid);
            }
        }
        CloseHandle(token);
    }
    CloseHandle(handle);
}
void PrintProcess(const wchar_t* label, const ProcessContext& process) {
    static std::mutex outputMutex;
    std::lock_guard<std::mutex> lock(outputMutex);
    std::wcout << L"[" << label << L"] PID=" << process.pid
               << L" ParentPID=" << process.parentPid
               << L" Key=" << process.uniqueProcessKey
               << L" Image=" << process.image;
    if (process.exitCode) std::wcout << L" ExitCode=" << *process.exitCode;
    std::wcout << std::endl;
}
bool BootstrapRootProcess(HandlerContext& context) {
    ProcessContext process;
    process.pid = context.managedProcess.processId;
    process.parentPid = GetCurrentProcessId();
    process.commandLine = context.managedProcess.commandLine;
    EnrichLiveProcess(process);
    auto result = context.store.Register(context.managedProcess, process);
    if (!result) return false;
    if (result->inserted) PrintProcess(L"PROCESS EXISTING", *context.store.Find(result->key));
    return true;
}
void RegisterProcessEvent(krabs::parser& parser, HandlerContext& context, const wchar_t* label) {
    ProcessContext process;
    FILETIME created{};
    if (!parser.try_parse(L"ProcessID", process.pid) ||
        !parser.try_parse(L"CreateTime", created) || !FileTimeTicks(created)) return;
    if (!IsProcessInJob(context.managedProcess.job, process.pid)) return;
    process.createTimeTicks = FileTimeTicks(created);
    process.startTime = FromFileTime(created);
    parser.try_parse(L"ParentProcessID", process.parentPid);
    parser.try_parse(L"ProcessSequenceNumber", process.processSequenceNumber);
    parser.try_parse(L"ImageName", process.image);
    krabs::sid integrity;
    if (parser.try_parse(L"MandatoryLabel", integrity)) {
        process.integritySid.assign(integrity.sid_string.begin(), integrity.sid_string.end());
    }
    EnrichLiveProcess(process);
    auto result = context.store.Register(context.managedProcess, process);
    if (result && result->inserted) PrintProcess(label, *context.store.Find(result->key));
}
std::optional<uint64_t> ResolveProcess(krabs::parser& parser, HandlerContext& context) {
    uint32_t pid = 0;
    if (!parser.try_parse(L"ProcessID", pid) || !IsProcessInJob(context.managedProcess.job, pid)) return {};
    auto key = context.store.FindProcessKey(pid);
    if (key) return key;
    ProcessContext process;
    process.pid = pid;
    EnrichLiveProcess(process);
    auto result = context.store.Register(context.managedProcess, process);
    if (!result) return {};
    if (result->inserted) PrintProcess(L"PROCESS EXISTING", *context.store.Find(result->key));
    return result->key;
}
bool ShouldPrintImage(const std::wstring& path) {
    return !path.empty() && !IsSameOrInsideFolder(path, L"C:\\Windows");
}
