#pragma once
#include "../../../krabs/krabs.hpp"
#include "../../../ProcessContext/ProcessContext.h"
#include "../../../StartProcess/StartProcess.h"
#include <optional>

struct HandlerContext {
    ManagedJobProcess& managedProcess;
    ProcessContextStore& store;
};
uint64_t FileTimeTicks(const FILETIME& time);
TimePoint FromFileTime(const FILETIME& time);
// Reads only the live process matching the supplied creation time, preventing PID reuse mixups.
void EnrichLiveProcess(ProcessContext& process);
bool BootstrapRootProcess(HandlerContext& context);
void RegisterProcessEvent(krabs::parser& parser, HandlerContext& context, const wchar_t* label);
std::optional<uint64_t> ResolveProcess(krabs::parser& parser, HandlerContext& context);
void PrintProcess(const wchar_t* label, const ProcessContext& process);
bool ShouldPrintImage(const std::wstring& path);
