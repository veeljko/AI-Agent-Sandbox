#include "OperationEndHandler.h"
#include "../Common/HandlerCommon.h"
#include <utility>

using namespace FileHandlerCommon;

bool OperationEndHandler(krabs::parser& parser) {
    std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
    uint64_t irp = 0;
    uint32_t status = 0;
    if (!TryParsePointer(parser, L"Irp", irp)) {
        TryParsePointer(parser, L"IrpPtr", irp);
    }
    const bool hasStatus = TryParseOperationStatus(parser, status);

    auto it = pending_protected_creates.find(irp);
    if (it == pending_protected_creates.end()) {
        return true;
    }

    PendingProtectedCreate pending = std::move(it->second);
    pending_protected_creates.erase(it);
    if (!IsCompletedFileOperationSuccessful(hasStatus, status)) {
        return true;
    }
    if (IsExistingDirectory(pending.path)) {
        return true;
    }

    if (pending.fileObject != 0) {
        file_object_to_path[pending.fileObject] = pending.path;
    }
    PrintAccess(
        L"PROTECTED CREATE/OPEN",
        pending.path,
        pending.processId
    );
    return false;
}
