For Event Tracing, I used this library: https://github.com/microsoft/krabsetw  
For starting and configuring containers, I used this library: https://github.com/containerd/containerd  
I used the Windows API to start processes and monitor their child processes.  

https://github.com/user-attachments/assets/44465c31-476a-480b-97d2-0e78ebc443df

## Windows build (CMake + Ninja)

Potreban je MSVC sa C++17 podrškom, Windows SDK, CMake 3.20 ili noviji, Ninja
i Go verzija navedena u `HCS/go.mod` ili novija (trenutno najmanje 1.20).
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
`where cl`, `cmake --version`, `ninja --version` i `go version`.

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

Oba preseta grade i Go programe iz `HCS/cmd/`, uz zavisnosti iz postojećih
`HCS/go.mod` i `HCS/go.sum`. Prvi build preuzima te module i zahteva mrežni
pristup. Go cache, preuzeti moduli i privremeni objekti ostaju u
`build/<konfiguracija>/HCS/go/`; izvorni `go.mod` i `go.sum` se ne menjaju.
Debug isključuje Go optimizacije i inlining (`-gcflags="all=-N -l"`), a Release
koristi podrazumevane Go optimizacije. Arhitektura prati izabrani MSVC target.

| Program | Debug | Release |
| --- | --- | --- |
| Monitor | `build/debug/main.exe` | `build/release/main.exe` |
| HCS runner | `build/debug/HCS/sandbox-runner.exe` | `build/release/HCS/sandbox-runner.exe` |
| Workspace helper | `build/debug/HCS/workspace-delete.exe` | `build/release/HCS/workspace-delete.exe` |

Uz Go programe kopiraju se `HCS/hcs-workspace.config.toml` i
`HCS/rules/hcs-workspace.rules`, uz očuvanje relativnih putanja koje monitor
očekuje. Build ne pokreće monitor, ne inicijalizuje HCS okruženje i ne kreira
kontejnere; za izvršavanje ostaju potrebni postojeći containerd/HCS setup,
pripremljeni image i namenski Codex home.

Postojeće `.bat` skripte ostaju zaseban način buildovanja; za gore navedeni
raspored izlaznih fajlova koristi CMake komande.

Provereni su kompletni Debug i Release buildovi na Windows x64 sa MSVC
19.51.36243, Windows SDK 10.0.26100.0, CMake 4.2.3-msvc3, Ninja 1.13.2 i
Go 1.26.3. Ponovljeni buildovi bez izmena prijavljuju `ninja: no work to do.`



