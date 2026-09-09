#include "NameDeleteHandler.h"
#include "../Common/HandlerCommon.h"

using namespace FileHandlerCommon;

bool NameDeleteHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t fileKey = 0;
    if (!TryParsePointer(parser, L"FileKey", fileKey) || fileKey == 0) {
        return true;
    }
    file_key_to_path.erase(fileKey);
    return true;
}
