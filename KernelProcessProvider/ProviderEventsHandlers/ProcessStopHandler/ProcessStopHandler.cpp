#include "ProcessStopHandler.h"

namespace KernelProcessHandlers {
void ProcessStopHandler(krabs::parser& parser, HandlerContext& context) {
    uint32_t pid = 0;
    FILETIME created{}, exited{};
    if (!parser.try_parse(L"ProcessID", pid) || !parser.try_parse(L"CreateTime", created) ||
        !FileTimeTicks(created)) return;
    // Match the exact lifetime; a late stop must not stop a reused PID.
    auto key = context.store.FindProcessKey(pid, FileTimeTicks(created));
    if (!key) return;
    uint32_t exitCode = 0;
    const bool hasExitCode = parser.try_parse(L"ExitCode", exitCode);
    const bool hasExitTime = parser.try_parse(L"ExitTime", exited);
    auto previous = context.store.Find(*key);
    if (!previous || !previous->running) return;
    context.store.Update(*key, [&](ProcessContext& process) {
        process.running = false;
        if (hasExitCode) process.exitCode = exitCode;
        if (hasExitTime) process.exitTime = FromFileTime(exited);
        for (auto& thread : process.threads) thread.running = false;
        for (auto& image : process.images) {
            if (image.loaded) { image.loaded = false; image.unloadTime = process.exitTime; }
        }
    });
    PrintProcess(L"PROCESS STOP", *context.store.Find(*key));
}
}
