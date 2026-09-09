#include "HandlerCommon.h"
#include "../../../StartProcess/StartProcess.h"
#include "../../../FilterFiles/FilterFiles.h"
#include <iostream>

namespace FileHandlerCommon {
std::mutex mutex;
std::map<uint64_t, std::wstring> file_object_to_path;
std::map<uint64_t, PendingProtectedCreate> pending_protected_creates;
static int counter = 0;
static const std::wstring workingDirStr = NormalizeFilePath(workingDir);

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
