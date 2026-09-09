#include "CreateOpenHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool CreateOpenHandler(krabs::parser& parser, uint32_t processId){
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    std::wstring filePath;
    uint64_t irp = 0;
    uint64_t fileObject = 0;
    uint32_t raw = 0;

    parser.try_parse(L"FileName", filePath);
    parser.try_parse(L"CreateOptions", raw);
    if (!TryParsePointer(parser, L"Irp", irp)) {
        TryParsePointer(parser, L"IrpPtr", irp);
    }
    TryParsePointer(parser, L"FileObject", fileObject);

    if (IsDirectoryCreateOpenOptions(raw)) return true;

    std::wstring normalizedPath = NormalizeFilePath(filePath);
    if (fileObject != 0 && !filePath.empty()) {
        file_object_to_path[fileObject] = normalizedPath;
    }

    if (IsProtectedOutsideWorkingDir(normalizedPath) && irp != 0) {
        pending_protected_creates[irp] = {
            normalizedPath,
            fileObject,
            processId
        };
    }
    
    return true;
}
