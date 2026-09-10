#include "ThreadStopHandler.h"
#include <iostream>

namespace KernelProcessHandlers {
void ThreadStopHandler(krabs::parser& parser, HandlerContext& context) {
    auto key = ResolveProcess(parser, context);
    uint32_t tid = 0;
    if (!key || !parser.try_parse(L"ThreadID", tid)) return;
    bool changed = false;
    context.store.Update(*key, [&](ProcessContext& process) {
        for (auto& thread : process.threads) {
            if (thread.tid == tid && thread.running) { thread.running = false; changed = true; }
        }
    });
    if (changed) std::wcout << L"[THREAD STOP] Key=" << *key << L" TID=" << tid << std::endl;
}
}
