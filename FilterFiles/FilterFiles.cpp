#include "FilterFiles.h"

#include "../NormalizePath/NormalizePath.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <filesystem>
#include <vector>
#include <string>
#include <cwctype>

namespace{
    std::wstring ToLowerPath(std::wstring s) {
        for (wchar_t& ch : s) {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }
        return s;
    }

    std::wstring RemoveTrailingSlash(std::wstring path) {
        while (path.size() > 3 &&
            (path.back() == L'\\' || path.back() == L'/')) {
            path.pop_back();
        }
        return path;
    }

    std::wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
        PWSTR rawPath = nullptr;

        HRESULT hr = SHGetKnownFolderPath(
            folderId,
            0,
            nullptr,
            &rawPath
        );

        if (FAILED(hr) || rawPath == nullptr) {
            return L"";
        }

        std::wstring result(rawPath);
        CoTaskMemFree(rawPath);

        return NormalizeFilePath(result);
    }
}


bool IsSameOrInsideFolder(
    const std::wstring& normalizedPath,
    const std::wstring& normalizedFolder
) {
    if (normalizedPath.empty() || normalizedFolder.empty()) {
        return false;
    }

    std::wstring path = RemoveTrailingSlash(NormalizeFilePath(normalizedPath));
    std::wstring dir = RemoveTrailingSlash(NormalizeFilePath(normalizedFolder));

    path = ToLowerPath(path);
    dir = ToLowerPath(dir);

    // Isti folder/fajl kao root.
    if (path == dir) {
        return true;
    }

    // Mora da bude "dir\nešto", ne samo "dirXYZ".
    if (path.size() <= dir.size()) {
        return false;
    }

    if (path.compare(0, dir.size(), dir) != 0) {
        return false;
    }

    wchar_t nextChar = path[dir.size()];
    return nextChar == L'\\' || nextChar == L'/';
}

bool IsPathInProtectedPersonalFolders(const std::wstring& normalizedPath) {
    static std::vector<std::wstring> protectedFolders = {
        GetKnownFolderPath(FOLDERID_Desktop),
        GetKnownFolderPath(FOLDERID_Documents),
        GetKnownFolderPath(FOLDERID_Pictures),
        GetKnownFolderPath(FOLDERID_Videos)
    };

    for (const std::wstring& folder : protectedFolders) {
        if (IsSameOrInsideFolder(normalizedPath, folder)) {
            return true;
        }
    }

    return false;
}

bool IsPathAllowed(const std::wstring& normalizedPath) {
    return !IsPathInProtectedPersonalFolders(normalizedPath);
}
