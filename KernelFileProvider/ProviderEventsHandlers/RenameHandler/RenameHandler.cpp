#include "RenameHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool RenameHandler(krabs::parser& parser, uint32_t processId){
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t fileObject = 0;
    TryParsePointer(parser, L"FileObject", fileObject);

    auto it = file_object_to_path.find(fileObject);
    std::wstring oldPath = L"";
    if (it != file_object_to_path.end()) {
        oldPath = it->second;

        if (IsProtectedOutsideWorkingDir(oldPath)) {
            // PrintRename(oldPath, L"", processId);
            return false;
        }
    }
    
    return true;
}
