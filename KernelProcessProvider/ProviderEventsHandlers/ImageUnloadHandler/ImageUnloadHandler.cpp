#include "ImageUnloadHandler.h"
#include <iostream>

namespace KernelProcessHandlers {
void ImageUnloadHandler(krabs::parser& parser, HandlerContext& context) {
    auto key = ResolveProcess(parser, context);
    krabs::pointer base{};
    if (!key || !parser.try_parse(L"ImageBase", base)) return;
    std::wstring path;
    context.store.Update(*key, [&](ProcessContext& process) {
        for (auto& image : process.images) {
            if (image.loaded && image.baseAddress == base.address) {
                image.loaded = false;
                image.unloadTime = std::chrono::system_clock::now();
                path = image.path;
            }
        }
    });
    if (ShouldPrintImage(path)) std::wcout << L"[IMAGE UNLOAD] Key=" << *key << L" Path=" << path << std::endl;
}
}
