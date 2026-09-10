#include "ThreadStartHandler.h"
#include <algorithm>
#include <iostream>

namespace KernelProcessHandlers {
void ThreadStartHandler(krabs::parser& parser, HandlerContext& context) {
    auto key = ResolveProcess(parser, context);
    uint32_t tid = 0;
    if (!key || !parser.try_parse(L"ThreadID", tid) || !tid) return;
    krabs::pointer address{};
    parser.try_parse(L"Win32StartAddr", address);
    bool inserted = false;
    context.store.Update(*key, [&](ProcessContext& process) {
        if (!process.running) return;
        auto found = std::find_if(process.threads.begin(), process.threads.end(),
            [tid](const ThreadContext& thread) { return thread.tid == tid && thread.running; });
        if (found == process.threads.end()) {
            process.threads.push_back({tid, address.address, true});
            inserted = true;
        }
    });
    if (inserted) std::wcout << L"[THREAD START] Key=" << *key << L" TID=" << tid << std::endl;
}
}
