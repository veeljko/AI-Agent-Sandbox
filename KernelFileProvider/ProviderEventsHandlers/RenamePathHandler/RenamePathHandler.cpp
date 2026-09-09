#include "RenamePathHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool RenamePathHandler(krabs::parser& parser, uint32_t processId){
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    std::wstring newPath;
    uint64_t fileObject = 0;

    parser.try_parse(L"FilePath", newPath);
    TryParsePointer(parser, L"FileObject", fileObject);

    std::wstring oldPath = ResolveFilePath(parser);

    std::wstring normalizedNewPath = NormalizeFilePath(newPath);
    bool renamedFromProtectedFolder = IsProtectedOutsideWorkingDir(oldPath);
    bool renamedToProtectedFolder = IsProtectedOutsideWorkingDir(normalizedNewPath);

    PrintRename(oldPath, normalizedNewPath, processId);

    if (fileObject != 0 && !newPath.empty()) {
        file_object_to_path[fileObject] = normalizedNewPath;
    }

    return !(renamedFromProtectedFolder || renamedToProtectedFolder);
}
