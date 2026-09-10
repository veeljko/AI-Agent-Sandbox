#include "FileEventRecorder.h"
#include "../../../ProcessContext/ProcessContext.h"

void RecordFileEvent(uint16_t eventId, krabs::parser& parser, uint32_t processId) {
    const wchar_t* operation = nullptr;
    switch (eventId) {
    case 12: operation = L"CREATE/OPEN REQUEST"; break;
    case 14: operation = L"CLOSE"; break;
    case 15: operation = L"READ REQUEST"; break;
    case 16: operation = L"WRITE REQUEST"; break;
    case 17: operation = L"SETINFORMATION REQUEST"; break;
    case 18: operation = L"SETDELETE REQUEST"; break;
    case 19: case 29: operation = L"RENAME REQUEST"; break;
    case 26: operation = L"DELETEPATH"; break;
    case 27: operation = L"RENAMEPATH"; break;
    case 30: operation = L"CREATE NEW FILE"; break;
    default: return;
    }
    auto& store = GetProcessContextStore();
    auto key = store.FindProcessKey(processId);
    if (!key) return; // Only KernelProcessProvider creates process identities.
    FileEvent event;
    event.eventId = eventId;
    event.operation = operation;
    event.timestamp = std::chrono::system_clock::now(); // Observation time.
    if (eventId == 12 || eventId == 30) {
        parser.try_parse(L"FileName", event.path);
    } else if (eventId == 26 || eventId == 27) {
        parser.try_parse(L"FilePath", event.path);
    }
    if (event.path.empty()) {
        std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
        event.path = FileHandlerCommon::ResolveFilePath(parser);
    }
    event.path = NormalizeFilePath(event.path);
    if (eventId == 12) {
        uint32_t options = 0;
        if (parser.try_parse(L"CreateOptions", options) && IsDeleteOnCloseOptions(options)) {
            event.operation = L"DELETE ON CLOSE REQUEST";
        }
    }
    if (eventId == 15 || eventId == 16) {
        uint64_t offset = 0;
        uint32_t size = 0;
        if (parser.try_parse(L"ByteOffset", offset)) event.byteOffset = offset;
        if (parser.try_parse(L"IOSize", size)) event.ioSize = size;
    }
    // Console filters do not discard the underlying history, including unknown paths.
    store.Update(*key, [&](ProcessContext& process) { process.files.push_back(event); });
}
