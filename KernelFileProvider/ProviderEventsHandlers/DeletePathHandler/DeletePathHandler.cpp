#include "DeletePathHandler.h"
#include "../Common/HandlerCommon.h"
#include <iostream>

using namespace FileHandlerCommon;

bool DeletePathHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t fileObject = 0;
    TryParsePointer(parser, L"FileObject", fileObject);
    std::wstring filePath;
    auto found = file_object_to_path.find(fileObject);
    if (found != file_object_to_path.end()) {
        filePath = found->second;
    }
    parser.try_parse(L"FilePath", filePath);
    PrintAccess(L"DELETEPATH", filePath, processId);
    // Diagnostic only; do not infer a policy violation from an I/O request.
    return true;
}
