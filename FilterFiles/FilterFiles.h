#pragma once
#include <string>

bool IsSameOrInsideFolder(const std::wstring& path, const std::wstring& folder);
bool IsPathInProtectedPersonalFolders(const std::wstring& normalizedPath);
bool IsPathAllowed(const std::wstring& normalizedPath);
