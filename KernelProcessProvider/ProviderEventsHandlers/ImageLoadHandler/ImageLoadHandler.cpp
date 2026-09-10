#include "ImageLoadHandler.h"
#include <algorithm>
#include <iostream>

namespace KernelProcessHandlers {
void ImageLoadHandler(krabs::parser& parser, HandlerContext& context) {
    auto key = ResolveProcess(parser, context);
    if (!key) return;
    LoadedImage image;
    krabs::pointer base{}, size{};
    if (!parser.try_parse(L"ImageBase", base) || !base.address) return;
    parser.try_parse(L"ImageSize", size);
    parser.try_parse(L"ImageName", image.path);
    image.baseAddress = base.address;
    image.size = size.address;
    image.timestamp = std::chrono::system_clock::now(); // Observation time, not raw ETW/QPC time.
    bool inserted = false;
    context.store.Update(*key, [&](ProcessContext& process) {
        if (!process.running) return;
        auto found = std::find_if(process.images.begin(), process.images.end(),
            [&](const LoadedImage& loaded) { return loaded.loaded && loaded.baseAddress == image.baseAddress; });
        if (found == process.images.end()) { process.images.push_back(image); inserted = true; }
    });
    if (inserted && ShouldPrintImage(image.path)) {
        std::wcout << L"[IMAGE LOAD] Key=" << *key << L" Path=" << image.path << L" Size=" << image.size << std::endl;
    }
}
}
