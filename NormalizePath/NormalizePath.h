#pragma once

#include <cstddef>
#include <string>

bool StartsWithIgnoreCase(const std::wstring& value, const std::wstring& prefix);
void ReplaceAll(std::wstring& s, wchar_t from, wchar_t to);
std::wstring RemovePrefix(const std::wstring& s, const std::wstring& prefix);
std::wstring GetFullPathSafe(const std::wstring& path);
bool IsBoundaryAfterPrefix(const std::wstring& path, size_t prefixLength);
std::wstring DevicePathToDosPath(const std::wstring& inputPath);
std::wstring NormalizeFilePath(const std::wstring& originalPath);
