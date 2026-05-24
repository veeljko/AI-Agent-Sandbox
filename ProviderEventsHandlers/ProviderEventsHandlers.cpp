#include "ProviderEventsHandlers.h"
#include "../NormalizePath/NormalizePath.h"
#include "../StartProcess/StartProcess.h"
#include "../FilterFiles/FilterFiles.h"

#include <iostream>
#include <map>

static const std::wstring targetPath = NormalizeFilePath(L"C:\\Users\\Korisnik\\Desktop\\test\\test.txt");
static const std::wstring workingDirStr = NormalizeFilePath(workingDir);

namespace{
    constexpr uint32_t kFileDirectoryFile = 0x00000001;

    static int counter = 0;
    static std::map<uint64_t, std::wstring> file_object_to_path;


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

    bool IsDirectoryCreateOpen(uint32_t& raw) {
        uint32_t createOptions = raw & 0x00FFFFFF;
        return (createOptions & 0x00000040) == 0;
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
}

bool IsValidDir(const std::wstring &path){
    return !IsProtectedOutsideWorkingDir(path);
}

// 12 = Create/Open
bool CreateOpenHandler(krabs::parser& parser, uint32_t processId){
    std::wstring filePath;
    uint64_t fileObject = 0;
    uint32_t raw = 0;

    parser.try_parse(L"FileName", filePath);
    parser.try_parse(L"CreateOptions", raw);
    TryParsePointer(parser, L"FileObject", fileObject);

    if (IsDirectoryCreateOpen(raw)) return true;

    std::wstring normalizedPath = NormalizeFilePath(filePath);
    if (fileObject != 0 && !filePath.empty()) {
        file_object_to_path[fileObject] = normalizedPath;
    }

    if (IsProtectedOutsideWorkingDir(normalizedPath)) {
        // PrintAccess(L"PROTECTED CREATE/OPEN", normalizedPath, processId);
        return false;
    }
    
    return true;
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
