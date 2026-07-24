#include "ProviderEventsHandlers.h"
#include "FileCreatePolicy.h"
#include "../NormalizePath/NormalizePath.h"
#include "../StartProcess/StartProcess.h"
#include "../FilterFiles/FilterFiles.h"

#include <iostream>
#include <map>

static const std::wstring targetPath = NormalizeFilePath(L"C:\\Users\\Korisnik\\Desktop\\test\\test.txt");
static const std::wstring workingDirStr = NormalizeFilePath(workingDir);

namespace{
    static int counter = 0;
    static std::map<uint64_t, std::wstring> file_object_to_path;

    struct PendingProtectedCreate {
        std::wstring path;
        uint64_t fileObject = 0;
        uint32_t processId = 0;
    };

    static std::map<uint64_t, PendingProtectedCreate> pending_protected_creates;

    bool IsTargetPath(const std::wstring& path){
        if (path.empty()) {
            return false;
        }

        std::wstring normalizedPath = NormalizeFilePath(path);
        return _wcsicmp(normalizedPath.c_str(), targetPath.c_str()) == 0;
    };

    bool IsProtectedOutsideWorkingDir(const std::wstring& path) {
        if (path.empty()) {
            return false;
        }

        std::wstring normalizedPath = NormalizeFilePath(path);

        if (IsSameOrInsideFolder(normalizedPath, workingDirStr)) {
            return false;
        }

        return IsPathInProtectedPersonalFolders(normalizedPath);
    }

    bool IsExistingDirectory(const std::wstring& normalizedPath) {
        DWORD attributes = GetFileAttributesW(normalizedPath.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    void PrintAccess(const wchar_t* eventName, const std::wstring& path, uint32_t processId) {
        std::wcout << std :: endl<<L"[" << eventName << L"] " << counter++ << std :: endl;
        std::wcout << L"Process Id: " << processId << std :: endl;
        std::wcout << L"Path: " << NormalizeFilePath(path) << std :: endl;
    };

    void PrintRename(const std::wstring& oldPath, const std::wstring& newPath, uint32_t processId) {
        std::wcout << std :: endl << L"[RENAME]" << std :: endl;
        std::wcout << counter++ << std :: endl;
        std::wcout << L"Process Id: " << processId << std :: endl;

        if (!oldPath.empty()) {
            std::wcout << L"Old path: " << NormalizeFilePath(oldPath) << std :: endl;
        } else {
            std::wcout << L"Old path: <unknown>" << std :: endl;
        }

        if (!newPath.empty()) {
            std::wcout << L"New path: " << NormalizeFilePath(newPath) << std :: endl;
        } else {
            std::wcout << L"New path: <unknown>" << std :: endl;
        }
    };

    bool TryParsePointer(krabs::parser& parser, const wchar_t* name, uint64_t& out) {
        try {
            krabs::pointer ptr;
            if (parser.try_parse(name, ptr)) {
                out = ptr.address;
                return true;
            }
        } catch (...) {
        }

        try {
            uint64_t value = 0;
            if (parser.try_parse(name, value)) {
                out = value;
                return true;
            }
        } catch (...) {
        }

        try {
            uint32_t value = 0;
            if (parser.try_parse(name, value)) {
                out = value;
                return true;
            }
        } catch (...) {
        }

        return false;
    }

    bool TryParseOperationStatus(krabs::parser& parser, uint32_t& out) {
        // Microsoft-Windows-Kernel-File/Event 24 names this field "Status"
        // on current Windows builds. Keep the legacy name as a compatibility
        // fallback, but never treat a missing field as STATUS_SUCCESS.
        try {
            if (parser.try_parse(L"Status", out)) {
                return true;
            }
        } catch (...) {
        }

        try {
            if (parser.try_parse(L"NtStatus", out)) {
                return true;
            }
        } catch (...) {
        }

        return false;
    }
}

bool IsValidDir(const std::wstring &path){
    return !IsProtectedOutsideWorkingDir(path);
}

// 12 = Create/Open
bool CreateOpenHandler(krabs::parser& parser, uint32_t processId){
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

// 24 = OperationEnd. Create/Open events are emitted when the I/O starts, even
// for failed probes. Correlate by IRP and alert only after a successful result.
bool OperationEndHandler(krabs::parser& parser) {
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

// 30 = CreateNewFile. This event is emitted for an actual new file, so it can
// be handled directly without treating failed name probes as violations.
bool CreateNewFileHandler(krabs::parser& parser, uint32_t processId) {
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

// 27 = RenamePath
bool RenamePathHandler(krabs::parser& parser, uint32_t processId){
    std::wstring newPath;
    uint64_t fileObject = 0;

    parser.try_parse(L"FilePath", newPath);
    TryParsePointer(parser, L"FileObject", fileObject);

    std::wstring oldPath;

    auto it = file_object_to_path.find(fileObject);
    if (it != file_object_to_path.end()) {
        oldPath = it->second;
    }

    std::wstring normalizedNewPath = NormalizeFilePath(newPath);
    bool renamedFromProtectedFolder = IsProtectedOutsideWorkingDir(oldPath);
    bool renamedToProtectedFolder = IsProtectedOutsideWorkingDir(normalizedNewPath);

    // if (renamedFromProtectedFolder || renamedToProtectedFolder) {
    //     PrintRename(oldPath, normalizedNewPath, processId);
    // }

    if (fileObject != 0 && !newPath.empty()) {
        file_object_to_path[fileObject] = normalizedNewPath;
    }

    return !(renamedFromProtectedFolder || renamedToProtectedFolder);
}
// 19 = Rename
bool RenameHandler(krabs::parser& parser, uint32_t processId){
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
