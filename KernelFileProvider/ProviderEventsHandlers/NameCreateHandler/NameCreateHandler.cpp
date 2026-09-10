#include "NameCreateHandler.h"
#include "../Common/HandlerCommon.h"

using namespace FileHandlerCommon;

bool NameCreateHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t fileKey = 0;
    if (!TryParsePointer(parser, L"FileKey", fileKey) || fileKey == 0) {
        return true;
    }
    std::wstring filePath;
    if (parser.try_parse(L"FileName", filePath) && !filePath.empty()) {
        file_key_to_path[fileKey] = NormalizeFilePath(filePath);
    }
    return true;
}
