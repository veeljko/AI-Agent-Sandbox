#include "NormalizePath.h"

#include <windows.h>
#include <vector>
#include <algorithm>
#include <cwctype>

namespace{
    bool StartsWithIgnoreCase(const std::wstring& value, const std::wstring& prefix) {
        if (value.size() < prefix.size()) {
            return false;
        }

        return _wcsnicmp(value.c_str(), prefix.c_str(), prefix.size()) == 0;
    }

    void ReplaceAll(std::wstring& s, wchar_t from, wchar_t to) {
        std::replace(s.begin(), s.end(), from, to);
    }

    std::wstring RemovePrefix(const std::wstring& s, const std::wstring& prefix) {
        if (StartsWithIgnoreCase(s, prefix)) {
            return s.substr(prefix.size());
        }

        return s;
    }

    std::wstring GetFullPathSafe(const std::wstring& path) {
        DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

        if (needed == 0) {
            return path;
        }

        std::vector<wchar_t> buffer(needed + 1);

        DWORD result = GetFullPathNameW(
            path.c_str(),
            static_cast<DWORD>(buffer.size()),
            buffer.data(),
            nullptr
        );

        if (result == 0 || result >= buffer.size()) {
            return path;
        }

        return std::wstring(buffer.data());
    }

    bool IsBoundaryAfterPrefix(const std::wstring& path, size_t prefixLength) {
        if (path.size() == prefixLength) {
            return true;
        }

        wchar_t ch = path[prefixLength];

        return ch == L'\\' || ch == L'/';
    }

    std::wstring DevicePathToDosPath(const std::wstring& inputPath) {
        wchar_t drivesBuffer[512];

        DWORD len = GetLogicalDriveStringsW(
            static_cast<DWORD>(std::size(drivesBuffer)),
            drivesBuffer
        );

        if (len == 0 || len > std::size(drivesBuffer)) {
            return inputPath;
        }

        for (wchar_t* drive = drivesBuffer; *drive != L'\0'; drive += wcslen(drive) + 1) {
            std::wstring driveRoot = drive;
            std::wstring driveLetter = driveRoot.substr(0, 2); // "C:"

            wchar_t deviceNameBuffer[512];

            DWORD queryResult = QueryDosDeviceW(
                driveLetter.c_str(),
                deviceNameBuffer,
                static_cast<DWORD>(std::size(deviceNameBuffer))
            );

            if (queryResult == 0) {
                continue;
            }

            std::wstring deviceName = deviceNameBuffer; 

            if (StartsWithIgnoreCase(inputPath, deviceName) &&
                IsBoundaryAfterPrefix(inputPath, deviceName.size())) {
                
                std::wstring rest = inputPath.substr(deviceName.size());

                if (!rest.empty() && (rest[0] == L'\\' || rest[0] == L'/')) {
                    return driveLetter + rest;
                }

                return driveLetter + L"\\" + rest;
            }
        }

        return inputPath;
    }
}

std::wstring NormalizeFilePath(const std::wstring& originalPath) {
    if (originalPath.empty()) {
        return originalPath;
    }

    std::wstring path = originalPath;

    ReplaceAll(path, L'/', L'\\');

    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.size() - 2);
    }

    // "\??\C:\Users\..." -> "C:\Users\..."
    path = RemovePrefix(path, L"\\??\\");

    // "\\?\C:\Users\..." -> "C:\Users\..."
    path = RemovePrefix(path, L"\\\\?\\");

    // "\\?\UNC\server\share\file.txt" -> "\\server\share\file.txt"
    if (StartsWithIgnoreCase(path, L"UNC\\")) {
        path = L"\\\\" + path.substr(4);
    }

    // "\SystemRoot\System32\..." -> "C:\Windows\System32\..."
    if (StartsWithIgnoreCase(path, L"\\SystemRoot\\")) {
        wchar_t windowsDir[MAX_PATH];

        UINT result = GetWindowsDirectoryW(windowsDir, MAX_PATH);

        if (result > 0 && result < MAX_PATH) {
            path = std::wstring(windowsDir) + path.substr(wcslen(L"\\SystemRoot"));
        }
    }

    // "\Device\HarddiskVolume3\Users\..." -> "C:\Users\..."
    path = DevicePathToDosPath(path);

    if (path.size() >= 3 && path[1] == L':' && path[2] == L'\\') {
        path = GetFullPathSafe(path);
    }

    return path;
}
