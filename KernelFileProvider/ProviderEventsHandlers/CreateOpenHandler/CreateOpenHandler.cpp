#include "CreateOpenHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>
#include <iostream>
#include <iomanip>

using namespace FileHandlerCommon;

bool CreateOpenHandler(krabs::parser& parser, uint32_t processId){
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    std::wstring filePath;
    uint64_t irp = 0;
    uint64_t fileObject = 0;
    uint32_t raw = 0;

    parser.try_parse(L"FileName", filePath);
    const bool hasCreateOptions = parser.try_parse(L"CreateOptions", raw);
    if (!TryParsePointer(parser, L"Irp", irp)) {
        TryParsePointer(parser, L"IrpPtr", irp);
    }
    TryParsePointer(parser, L"FileObject", fileObject);

    std::wstring normalizedPath = NormalizeFilePath(filePath);
    if (fileObject != 0 && !filePath.empty()) {
        file_object_to_path[fileObject] = normalizedPath;
    }

    if (IsDirectoryCreateOpenOptions(raw)) return true;

    // Event 12 reports an attempt, not a successful read. Keep that distinction
    // visible even for allowed paths, so child-process opens can be diagnosed.
    const wchar_t* eventName = hasCreateOptions && IsDeleteOnCloseOptions(raw)
        ? L"DELETE ON CLOSE REQUEST" : L"CREATE/OPEN REQUEST";
    if (PrintAccess(eventName, normalizedPath, processId)) {
        if (hasCreateOptions) {
            std::wcout << L"CreateOptions: 0x" << std::hex << raw << std::dec << std::endl;
        } else {
            std::wcout << L"CreateOptions: <unavailable>" << std::endl;
        }
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
