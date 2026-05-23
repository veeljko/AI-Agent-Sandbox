#include "FilterFiles.h"

#include "../NormalizePath/NormalizePath.h"

#include <knownfolders.h>
#include <shlobj.h>
#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::wstring TrimTrailingSlashes(std::wstring path) {
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) {
        path.pop_back();
    }

    return path;
}

bool IsPathSeparator(wchar_t ch) {
    return ch == L'\\' || ch == L'/';
}

std::wstring ExpandEnvironmentPath(const wchar_t* path) {
    DWORD needed = ExpandEnvironmentStringsW(path, nullptr, 0);
    if (needed == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(needed);
    DWORD result = ExpandEnvironmentStringsW(path, buffer.data(), needed);
    if (result == 0 || result > needed) {
        return L"";
    }

    std::wstring expanded(buffer.data());
    if (expanded.find(L'%') != std::wstring::npos) {
        return L"";
    }

    return expanded;
}

void AddFolderIfPresent(std::vector<std::wstring>& folders, const std::wstring& folder) {
    if (folder.empty()) {
        return;
    }

    std::wstring normalizedFolder = TrimTrailingSlashes(NormalizeFilePath(folder));

    for (const auto& existingFolder : folders) {
        if (_wcsicmp(existingFolder.c_str(), normalizedFolder.c_str()) == 0) {
            return;
        }
    }

    folders.push_back(normalizedFolder);
}

}

bool IsSameOrInsideFolder(const std::wstring& path, const std::wstring& folder) {
    if (path.empty() || folder.empty()) {
        return false;
    }

    std::wstring normalizedPath = TrimTrailingSlashes(NormalizeFilePath(path));
    std::wstring normalizedFolder = TrimTrailingSlashes(NormalizeFilePath(folder));

    if (_wcsicmp(normalizedPath.c_str(), normalizedFolder.c_str()) == 0) {
        return true;
    }

    if (normalizedPath.size() <= normalizedFolder.size()) {
        return false;
    }

    if (_wcsnicmp(
            normalizedPath.c_str(),
            normalizedFolder.c_str(),
            normalizedFolder.size()) != 0) {
        return false;
    }

    return IsPathSeparator(normalizedPath[normalizedFolder.size()]);
}

std::wstring GetKnownFolderPathString(const KNOWNFOLDERID& folderId) {
    PWSTR rawPath = nullptr;

    HRESULT hr = SHGetKnownFolderPath(
        folderId,
        0,
        nullptr,
        &rawPath);

    if (FAILED(hr) || rawPath == nullptr) {
        return L"";
    }

    std::wstring result(rawPath);
    CoTaskMemFree(rawPath);

    return result;
}

std::vector<std::wstring> GetProtectedPersonalFolders() {
    std::vector<std::wstring> folders;

    std::vector<KNOWNFOLDERID> knownFolders = {
        FOLDERID_Desktop,
        FOLDERID_Documents,
        FOLDERID_Pictures,
        FOLDERID_Videos
    };

    for (const auto& folderId : knownFolders) {
        AddFolderIfPresent(folders, GetKnownFolderPathString(folderId));
    }

    AddFolderIfPresent(folders, ExpandEnvironmentPath(L"%USERPROFILE%\\Desktop"));
    AddFolderIfPresent(folders, ExpandEnvironmentPath(L"%USERPROFILE%\\Documents"));
    AddFolderIfPresent(folders, ExpandEnvironmentPath(L"%USERPROFILE%\\Pictures"));
    AddFolderIfPresent(folders, ExpandEnvironmentPath(L"%USERPROFILE%\\Videos"));

    return folders;
}

bool IsPathAllowed(const std::wstring& normalizedPath) {
    std::vector<std::wstring> forbiddenFolders = GetProtectedPersonalFolders();

    for (const auto& forbiddenPath : forbiddenFolders) {
        if (IsSameOrInsideFolder(normalizedPath, forbiddenPath)) {
            return false;
        }
    }

    return true;
}
