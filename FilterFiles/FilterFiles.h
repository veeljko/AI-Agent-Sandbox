#pragma once

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <string>
#include <vector>

bool IsSameOrInsideFolder(const std::wstring& path, const std::wstring& folder);
std::wstring GetKnownFolderPathString(const KNOWNFOLDERID& folderId);
std::vector<std::wstring> GetProtectedPersonalFolders();
bool IsPathAllowed(const std::wstring& normalizedPath);
