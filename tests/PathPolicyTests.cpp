#include "../FilterFiles/FilterFiles.h"
#include "../NormalizePath/NormalizePath.h"
#include "../ProviderEventsHandlers/FileCreatePolicy.h"

#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

namespace {
    std::wstring GetEnvironmentValue(const wchar_t* name) {
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
        return std::wstring(buffer.data(), written);
    }

    bool Expect(bool condition, const wchar_t* message) {
        if (condition) {
            return true;
        }
        std::wcerr << L"FAILED: " << message << std::endl;
        return false;
    }
}

int wmain() {
    bool passed = true;
    const std::wstring userProfile = GetEnvironmentValue(L"USERPROFILE");
    passed &= Expect(!userProfile.empty(), L"USERPROFILE must be available");

    const std::wstring legacyDesktopFile =
        NormalizeFilePath(userProfile + L"\\Desktop\\outside.txt");
    passed &= Expect(
        IsPathInProtectedPersonalFolders(legacyDesktopFile),
        L"the physical USERPROFILE Desktop must stay protected when Desktop is redirected"
    );
    passed &= Expect(
        IsPathInProtectedPersonalFolders(
            NormalizeFilePath(userProfile + L"\\Music\\outside.mp3")
        ),
        L"the physical USERPROFILE Music folder must be protected"
    );

    passed &= Expect(
        !IsSameOrInsideFolder(
            NormalizeFilePath(userProfile + L"\\Desktop\\outside.txt"),
            NormalizeFilePath(userProfile + L"\\Desktop\\test")
        ),
        L"a file in the Desktop parent must not be treated as inside the workspace"
    );

    passed &= Expect(
        !IsDirectoryCreateOpenOptions(0),
        L"create options without FILE_DIRECTORY_FILE must be checked as file access"
    );
    passed &= Expect(
        IsDirectoryCreateOpenOptions(0x00000001),
        L"FILE_DIRECTORY_FILE must identify a directory create/open"
    );
    passed &= Expect(
        !IsDirectoryCreateOpenOptions(0x00000040),
        L"FILE_NON_DIRECTORY_FILE must not be skipped as a directory"
    );
    passed &= Expect(
        IsSuccessfulNtStatus(0x00000000),
        L"STATUS_SUCCESS must be treated as a successful operation"
    );
    passed &= Expect(
        !IsSuccessfulNtStatus(0xC0000034),
        L"STATUS_OBJECT_NAME_NOT_FOUND must not trigger an access alert"
    );
    passed &= Expect(
        !IsSuccessfulNtStatus(0xC000003A),
        L"STATUS_OBJECT_PATH_NOT_FOUND must not trigger an access alert"
    );
    passed &= Expect(
        !IsCompletedFileOperationSuccessful(false, 0x00000000),
        L"a missing ETW status field must never default to STATUS_SUCCESS"
    );
    passed &= Expect(
        IsCompletedFileOperationSuccessful(true, 0x00000000),
        L"a parsed STATUS_SUCCESS must complete the protected-access check"
    );

    if (!passed) {
        return 1;
    }
    std::wcout << L"Path policy tests passed." << std::endl;
    return 0;
}
