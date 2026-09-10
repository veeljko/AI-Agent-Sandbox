#include "SetDeleteHandler.h"
#include "../Common/HandlerCommon.h"
#include <iostream>

using namespace FileHandlerCommon;

bool SetDeleteHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    const std::wstring filePath = ResolveFilePath(parser);
    if (!PrintAccess(L"SETDELETE REQUEST", filePath, processId)) {
        return true;
    }
    uint32_t infoClass = 0;
    if (parser.try_parse(L"InfoClass", infoClass)) {
        std::wcout << L"Info class: " << infoClass << std::endl;
    }
    // This event is a request; it does not establish that deletion succeeded.
    return true;
}
