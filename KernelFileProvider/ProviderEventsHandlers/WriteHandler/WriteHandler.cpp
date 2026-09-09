#include "WriteHandler.h"
#include "../Common/HandlerCommon.h"
#include <iostream>

using namespace FileHandlerCommon;

bool WriteHandler(krabs::parser& parser, uint32_t processId) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t fileObject = 0;
    TryParsePointer(parser, L"FileObject", fileObject);
    std::wstring filePath;
    auto found = file_object_to_path.find(fileObject);
    if (found != file_object_to_path.end()) {
        filePath = found->second;
    }
    PrintAccess(L"WRITE", filePath, processId);
    uint64_t byteOffset = 0;
    uint32_t ioSize = 0;
    if (parser.try_parse(L"ByteOffset", byteOffset)) {
        std::wcout << L"Byte offset: " << byteOffset << std::endl;
    }
    if (parser.try_parse(L"IOSize", ioSize)) {
        std::wcout << L"IO size: " << ioSize << std::endl;
    }
    // Diagnostic only; do not infer a policy violation from an I/O request.
    return true;
}
