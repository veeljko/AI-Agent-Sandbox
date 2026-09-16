#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

void Check(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) +
        " failed; Win32=" + std::to_string(GetLastError()));
}

// A correctly typed remote-thread entry point, present in our own test EXE.
// Only this test program's child is targeted; no PID or arbitrary DLL arguments.
DWORD WINAPI LoadTestDll(LPVOID path) {
    HMODULE module = LoadLibraryW(static_cast<LPCWSTR>(path));
    if (!module) return 0;
    Sleep(2000);
    return FreeLibrary(module) ? 1 : 0;
}

uintptr_t FindRemoteImage(DWORD pid, const std::wstring& image) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        Handle snapshot;
        snapshot.value = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snapshot.value != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (Module32FirstW(snapshot.value, &entry)) {
                do {
                    if (_wcsicmp(entry.szExePath, image.c_str()) == 0)
                        return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                } while (Module32NextW(snapshot.value, &entry));
            }
        }
        Sleep(50);
    }
    throw std::runtime_error("Could not locate test EXE in its child process.");
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring(argv[1]) == L"--target") {
        Handle done;
        done.value = OpenEventW(SYNCHRONIZE, FALSE, argv[2]);
        if (!done.value) return 2;
        std::wcout << L"[TARGET] PID=" << GetCurrentProcessId() << std::endl;
        return WaitForSingleObject(done.value, 30000) == WAIT_OBJECT_0 ? 0 : 3;
    }
    if (argc != 1) {
        std::cerr << "Run without arguments. The test creates its own target.\n";
        return 2;
    }

    Handle process, primaryThread, remoteThread, done;
    void* remotePath = nullptr;
    try {
        wchar_t executable[32768]{};
        DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
        Check(length && length < 32768, "GetModuleFileNameW");
        std::wstring image(executable, length);
        std::wstring dll = image.substr(0, image.find_last_of(L"\\/") + 1) + L"TestPayload.dll";
        Check(GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES, "Find TestPayload.dll");
        std::wstring eventName = L"Local\\KernelProviderDllTest_" +
            std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
        done.value = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
        Check(done.value != nullptr, "CreateEventW");

        std::wstring command = L"\"" + image + L"\" --target \"" + eventName + L"\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        Check(CreateProcessW(image.c_str(), command.data(), nullptr, nullptr, FALSE,
            0, nullptr, nullptr, &startup, &info) != FALSE, "CreateProcessW");
        process.value = info.hProcess;
        primaryThread.value = info.hThread;
        std::wcout << L"[INJECTOR] PID=" << GetCurrentProcessId()
            << L" TargetPID=" << info.dwProcessId << L"\n[DLL] " << dll << std::endl;
        Sleep(2000);

        // Resolve the helper by its RVA in the same EXE, allowing different ASLR bases.
        uintptr_t remoteBase = FindRemoteImage(info.dwProcessId, image);
        uintptr_t offset = reinterpret_cast<uintptr_t>(&LoadTestDll) -
            reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        auto remoteEntry = reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteBase + offset);
        SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
        remotePath = VirtualAllocEx(process.value, nullptr, bytes,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Check(remotePath != nullptr, "VirtualAllocEx");
        SIZE_T written = 0;
        Check(WriteProcessMemory(process.value, remotePath, dll.c_str(), bytes, &written)
            && written == bytes, "WriteProcessMemory");
        remoteThread.value = CreateRemoteThread(process.value, nullptr, 0,
            remoteEntry, remotePath, 0, nullptr);
        Check(remoteThread.value != nullptr, "CreateRemoteThread");
        Check(WaitForSingleObject(remoteThread.value, 10000) == WAIT_OBJECT_0,
            "Wait for remote thread");
        DWORD result = 0;
        Check(GetExitCodeThread(remoteThread.value, &result) != FALSE, "GetExitCodeThread");
        if (result != 1) throw std::runtime_error("Target failed to load/unload TestPayload.dll.");
        Check(VirtualFreeEx(process.value, remotePath, 0, MEM_RELEASE) != FALSE, "VirtualFreeEx");
        remotePath = nullptr;
        Check(SetEvent(done.value) != FALSE, "SetEvent");
        Check(WaitForSingleObject(process.value, 5000) == WAIT_OBJECT_0, "Wait for target exit");
        DWORD exitCode = 0;
        Check(GetExitCodeProcess(process.value, &exitCode) != FALSE, "GetExitCodeProcess");
        if (exitCode != 0) throw std::runtime_error("Target exited with an error.");
        std::cout << "[PASS] Remote thread loaded and unloaded TestPayload.dll; target exited successfully.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        // Only terminate the child created above. Its address space owns remotePath.
        // Do not free memory while a timed-out remote thread may still read it.
        if (process.value) {
            TerminateProcess(process.value, 1);
            WaitForSingleObject(process.value, 5000);
        }
        return 1;
    }
}
