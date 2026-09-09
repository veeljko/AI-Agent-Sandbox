#include "CreateNewFileHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool CreateNewFileHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    std::wstring filePath;
    uint64_t fileObject = 0;
    uint32_t rawCreateOptions = 0;
    parser.try_parse(L"FileName", filePath);
    parser.try_parse(L"CreateOptions", rawCreateOptions);
    TryParsePointer(parser, L"FileObject", fileObject);

    std::wstring normalizedPath = NormalizeFilePath(filePath);
    if (IsDirectoryCreateOpenOptions(rawCreateOptions) ||
        IsExistingDirectory(normalizedPath)) {
        return true;
    }
    if (fileObject != 0 && !normalizedPath.empty()) {
        file_object_to_path[fileObject] = normalizedPath;
    }
    if (!IsProtectedOutsideWorkingDir(normalizedPath)) {
        return true;
    }

    PrintAccess(L"PROTECTED CREATE NEW FILE", normalizedPath, processId);
    return false;
}
