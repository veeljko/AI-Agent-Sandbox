#pragma once

#include "../krabs/krabs.hpp"

#include <cstdint>
#include <string>

bool IsTargetPath(const std::wstring& path);
void PrintAccess(const wchar_t* eventName, const std::wstring& path, uint32_t processId);
void PrintRename(const std::wstring& oldPath, const std::wstring& newPath, uint32_t processId);
bool try_parse_pointer(krabs::parser& parser, const wchar_t* name, uint64_t& out);
void CreateOpenHandler(krabs::parser& parser, uint32_t processId);
void RenamePathHandler(krabs::parser& parser, uint32_t processId);
void Rename(krabs::parser& parser, uint32_t processId);
