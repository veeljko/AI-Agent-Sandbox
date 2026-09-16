#include <windows.h>

// Harmless payload: loading/unloading this DLL is the entire test.
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
