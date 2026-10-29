For Event Tracing, I used this library: https://github.com/microsoft/krabsetw  
For starting and configuring containers, I used this library: https://github.com/containerd/containerd  
I used the Windows API to start processes and monitor their child processes.  

https://github.com/user-attachments/assets/44465c31-476a-480b-97d2-0e78ebc443df

## Windows build (CMake + Ninja)

Potreban je MSVC sa C++17 podrškom, Windows SDK, CMake 3.20 ili noviji i Ninja.
Preseti koriste format 2, koji podržava configure i build presete od
[CMake 3.20](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html).
Priloženi `krabs/` je header-only biblioteka koja zahteva MSVC i `UNICODE`;
MinGW/GCC nije podržan ovim projektom. Ulazna tačka je `main.cpp`.
Linkuju se samo potrebne dodatne Windows SDK biblioteke: `tdh`, `advapi32`,
`ole32` i `shell32`.

Otvori **x64 Native Tools Command Prompt for VS 2026** (ili odgovarajući x64
developerski terminal svoje Visual Studio instalacije), pa pređi u koren
projekta. U Visual Studio Installer-u potrebni su C++ build alati, Windows SDK
i komponenta **C++ CMake tools for Windows**, koja uključuje CMake i Ninja.
Preseti pronalaze `cl`, CMake i Ninja preko developerskog okruženja, bez
apsolutnih putanja do instalacije. Dostupnost možeš proveriti komandama
`where cl`, `cmake --version` i `ninja --version`.

```bat
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

Debug koristi `CMAKE_BUILD_TYPE=Debug` i proizvodi `build/debug/main.exe`.
Release koristi `CMAKE_BUILD_TYPE=Release` i proizvodi `build/release/main.exe`.
Generator je običan `Ninja`; konfiguracije imaju zasebne cache, objektne i
izlazne fajlove. CMake odbija konfiguraciju direktno u izvornom direktorijumu.

Ovi preseti grade C++ monitor. HCS putanja izvršavanja dodatno zahteva Go
programe `sandbox-runner.exe` i `workspace-delete.exe` i pripadajuću konfiguraciju.
Postojeće `.bat` skripte ostaju zaseban način buildovanja; za gore navedeni
raspored izlaznih fajlova koristi CMake komande.



