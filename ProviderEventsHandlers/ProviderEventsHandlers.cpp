#include "ProviderEventsHandlers.h"

#include "../NormalizePath/NormalizePath.h"
#include <iostream>
#include <map>

static int counter = 0;
static const std::wstring targetPath = NormalizeFilePath(L"C:\\Users\\Korisnik\\Desktop\\test\\test.txt");
static std::map<uint64_t, std::wstring> file_object_to_path;

bool IsTargetPath(const std::wstring& path){
    if (path.empty()) {
        return false;
    }

    std::wstring normalizedPath = NormalizeFilePath(path);
    return _wcsicmp(normalizedPath.c_str(), targetPath.c_str()) == 0;
};

void PrintAccess(const wchar_t* eventName, const std::wstring& path, int32_t& processId) {
    std::wcout << L"\n[" << eventName << L"] " << counter++ << '\n';
    std::wcout << L"Process Id: " << processId << L"\n";
    std::wcout << L"Path: " << NormalizeFilePath(path) << L"\n";
};

void PrintRename(const std::wstring& oldPath, const std::wstring& newPath, int32_t& processId) {
    std::wcout << L"\n[RENAME]\n";
    std::wcout << counter++ << L"\n";
    std::wcout << L"Process Id: " << processId << L"\n";

    if (!oldPath.empty()) {
        std::wcout << L"Old path: " << NormalizeFilePath(oldPath) << L"\n";
    } else {
        std::wcout << L"Old path: <unknown>\n";
    }

    if (!newPath.empty()) {
        std::wcout << L"New path: " << NormalizeFilePath(newPath) << L"\n";
    } else {
        std::wcout << L"New path: <unknown>\n";
    }
};

bool try_parse_pointer(krabs::parser& parser, const wchar_t* name, uint64_t& out) {
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

// 12 = Create/Open
void CreateOpenHandler(krabs::parser& parser, int32_t processId){
    std::wstring filePath;
    uint64_t fileObject = 0;
    uint32_t raw = 0;

    parser.try_parse(L"FileName", filePath);
    parser.try_parse(L"CreateOptions", raw);
    try_parse_pointer(parser, L"FileObject", fileObject);

    if (fileObject != 0 && !filePath.empty()) {
        file_object_to_path[fileObject] = NormalizeFilePath(filePath);
    }

    if (IsTargetPath(filePath)) {
        PrintAccess(L"CREATE/OPEN test.txt", filePath, processId);
    }
}

// 27 = RenamePath
// Ovo je najkorisniji rename event, jer uglavnom ima FilePath.
void RenamePathHandler(krabs::parser& parser, int32_t processId){
    std::wstring newPath;
    uint64_t fileObject = 0;

    parser.try_parse(L"FilePath", newPath);
    try_parse_pointer(parser, L"FileObject", fileObject);

    std::wstring oldPath;

    auto it = file_object_to_path.find(fileObject);
    if (it != file_object_to_path.end()) {
        oldPath = it->second;
    }

    bool renamedFromTestTxt = IsTargetPath(oldPath);
    bool renamedToTestTxt = IsTargetPath(newPath);

    if (renamedFromTestTxt || renamedToTestTxt) {
        PrintRename(oldPath, newPath, processId);
    }

    // Posle rename-a, FileObject sada treba da pokazuje na novu putanju.
    if (fileObject != 0 && !newPath.empty()) {
        file_object_to_path[fileObject] = NormalizeFilePath(newPath);
    }
}
// 19 = Rename
// Ovaj event ne mora da ima novu putanju, ali može da kaže da je FileObject
// koji već znamo bio rename-ovan.
void Rename(krabs::parser& parser, int32_t processId){
    uint64_t fileObject = 0;
    try_parse_pointer(parser, L"FileObject", fileObject);

    auto it = file_object_to_path.find(fileObject);
    if (it != file_object_to_path.end()) {
        const std::wstring& oldPath = it->second;

        if (IsTargetPath(oldPath)) {
            PrintRename(oldPath, L"", processId);
        }
    }

}
