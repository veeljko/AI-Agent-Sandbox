#include "RenameHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool RenameHandler(krabs::parser& parser, uint32_t processId){
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    const std::wstring oldPath = ResolveFilePath(parser);
    PrintRename(oldPath, L"", processId);
    return !IsProtectedOutsideWorkingDir(oldPath);
}
