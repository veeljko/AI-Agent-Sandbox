# DLL injection provider test

Run `Run-DllInjectionTest.bat` from the CMD window created by the monitor.
The monitored JobObject must contain the test launcher and its child; running
from an unrelated terminal does not test this monitor's job-filtered output.

The script builds x64 binaries using the installed Visual Studio 18 Community
toolchain and puts them in `build/dll-injection-test`. Subsequent runs may launch
`build/dll-injection-test/DllInjectionTest.exe` directly to avoid compiler events.

The executable creates its own target (another instance of itself), allocates a
DLL-path buffer in that child, writes it with WriteProcessMemory, and starts a
remote thread. The thread runs a helper already compiled into the target EXE,
resolved by RVA against the target's actual image base. The helper loads the
harmless TestPayload.dll, waits two seconds, unloads it, and reports success.
This is a controlled remote-thread DLL-loading test, not manual mapping. It has
no option to inject into an arbitrary PID or to select another payload.

Expected monitor output:

- PROCESS START for the launcher and target (use the printed TargetPID).
- IMAGE LOAD for TestPayload.dll associated with the target's process Key.
- IMAGE UNLOAD for TestPayload.dll associated with the same Key.
- PROCESS STOP with ExitCode=0 for both processes.

The launcher prints PASS only after the target confirms load/unload and exits
successfully. That verifies the test operation, not ETW delivery. Check the
monitor output separately. IMAGE LOAD alone does not prove injection or identify
the injector. Thread callbacks remain disabled in KernelProcessProvider.cpp.
File activity on the DLL may also appear but is not a fixed read/write sequence.
