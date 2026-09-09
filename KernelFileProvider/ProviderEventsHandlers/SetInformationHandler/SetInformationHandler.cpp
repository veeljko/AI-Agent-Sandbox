#include "SetInformationHandler.h"
#include "../Common/HandlerCommon.h"
#include <iostream>

using namespace FileHandlerCommon;

bool SetInformationHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    const std::wstring filePath = ResolveFilePath(parser);
    if (!PrintAccess(L"SETINFORMATION REQUEST", filePath, processId)) {
        return true;
    }

    uint32_t infoClass = 0;
    if (parser.try_parse(L"InfoClass", infoClass)) {
        std::wcout << L"Info class: " << infoClass;
        // FILE_INFORMATION_CLASS values, not FILE_INFO_BY_HANDLE_CLASS values.
        if (infoClass == 13) {
            std::wcout << L" (FileDispositionInformation)";
        } else if (infoClass == 64) {
            std::wcout << L" (FileDispositionInformationEx)";
        }
        std::wcout << std::endl;
    }
    // Disposition information can set or clear deletion state. The manifest does
    // not expose the request's flags, so this is not proof of successful deletion.
    return true;
}
