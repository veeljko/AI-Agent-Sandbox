#include "FilterFiles.h"

#include "../NormalizePath/NormalizePath.h"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <vector>
#include <string>
#include <cwctype>
#include <utility>

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

    std::wstring GetEnvironmentPath(const wchar_t* name) {
        DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0) {
            return L"";
        }

        std::vector<wchar_t> buffer(required);
        DWORD written = GetEnvironmentVariableW(
            name,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );
        if (written == 0 || written >= buffer.size()) {
            return L"";
        }
        return NormalizeFilePath(std::wstring(buffer.data(), written));
    }

    void AddUniqueFolder(
        std::vector<std::wstring>& folders,
        const std::wstring& folder
    ) {
        if (folder.empty()) {
            return;
        }

        std::wstring normalizedFolder =
            RemoveTrailingSlash(NormalizeFilePath(folder));
        for (const std::wstring& existing : folders) {
            if (_wcsicmp(existing.c_str(), normalizedFolder.c_str()) == 0) {
                return;
            }
        }
        folders.push_back(std::move(normalizedFolder));
    }

    std::vector<std::wstring> BuildProtectedPersonalFolders() {
        std::vector<std::wstring> folders;
        AddUniqueFolder(folders, GetKnownFolderPath(FOLDERID_Desktop));
        AddUniqueFolder(folders, GetKnownFolderPath(FOLDERID_Documents));
        AddUniqueFolder(folders, GetKnownFolderPath(FOLDERID_Pictures));
        AddUniqueFolder(folders, GetKnownFolderPath(FOLDERID_Videos));
        AddUniqueFolder(folders, GetKnownFolderPath(FOLDERID_Music));

        const std::wstring userProfile = GetEnvironmentPath(L"USERPROFILE");
        if (!userProfile.empty()) {
            AddUniqueFolder(folders, userProfile + L"\\Desktop");
            AddUniqueFolder(folders, userProfile + L"\\Documents");
            AddUniqueFolder(folders, userProfile + L"\\Pictures");
            AddUniqueFolder(folders, userProfile + L"\\Videos");
            AddUniqueFolder(folders, userProfile + L"\\Music");
        }
        return folders;
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
    static const std::vector<std::wstring> protectedFolders =
        BuildProtectedPersonalFolders();

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
