#pragma once
#include "../../../krabs/krabs.hpp"
#include "../../../NormalizePath/NormalizePath.h"
#include "../FileCreatePolicy.h"
#include <map>
#include <mutex>
#include <string>

namespace FileHandlerCommon {
struct PendingProtectedCreate {
    std::wstring path;
    uint64_t fileObject = 0;
    uint32_t processId = 0;
};
// Handlers lock this mutex before accessing shared correlation state.
extern std::mutex mutex;
extern std::map<uint64_t, std::wstring> file_object_to_path;
extern std::map<uint64_t, PendingProtectedCreate> pending_protected_creates;
bool IsProtectedOutsideWorkingDir(const std::wstring& path);
bool IsExistingDirectory(const std::wstring& path);
void PrintAccess(const wchar_t* eventName, const std::wstring& path, uint32_t processId);
void PrintRename(const std::wstring& oldPath, const std::wstring& newPath, uint32_t processId);
bool TryParsePointer(krabs::parser& parser, const wchar_t* name, uint64_t& out);
bool TryParseOperationStatus(krabs::parser& parser, uint32_t& out);
}
