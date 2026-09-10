@echo off
setlocal

set "PROJECT_INPUT_SOURCE=%~1"
set "PROJECT_OUTPUT_EXE=%~2"
if not defined PROJECT_INPUT_SOURCE set "PROJECT_INPUT_SOURCE=%~dp0main.cpp"
if not defined PROJECT_OUTPUT_EXE set "PROJECT_OUTPUT_EXE=%~dp0main.exe"
for %%F in ("%PROJECT_OUTPUT_EXE%") do set "PROJECT_OUTPUT_NAME=%%~nF"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%

set "BUILD_OBJ_DIR=%~dp0build\%PROJECT_OUTPUT_NAME%\obj"
if not exist "%BUILD_OBJ_DIR%" mkdir "%BUILD_OBJ_DIR%"
if errorlevel 1 exit /b %errorlevel%

set "PROCESS_CONTEXT_OBJ=%BUILD_OBJ_DIR%\ProcessContext.obj"
set "MAIN_OBJ=%BUILD_OBJ_DIR%\main.obj"
set "HCS_SANDBOX_OBJ=%BUILD_OBJ_DIR%\HcsSandbox.obj"
set "START_PROCESS_OBJ=%BUILD_OBJ_DIR%\StartProcess.obj"
set "NORMALIZE_PATH_OBJ=%BUILD_OBJ_DIR%\NormalizePath.obj"
set "FILTER_FILES_OBJ=%BUILD_OBJ_DIR%\FilterFiles.obj"
set "KERNEL_FILE_PROVIDER_OBJ=%BUILD_OBJ_DIR%\KernelFileProvider.obj"
call "%~dp0kernel-file-handler-sources.bat"
call "%~dp0kernel-process-handler-sources.bat"
set "PDB=%BUILD_OBJ_DIR%\compiler.pdb"

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "%PROJECT_INPUT_SOURCE%" ^
  /Fo"%MAIN_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0." ^
  /c "HcsSandbox\HcsSandbox.cpp" ^
  /Fo"%HCS_SANDBOX_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "StartProcess\StartProcess.cpp" ^
  /Fo"%START_PROCESS_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "NormalizePath\NormalizePath.cpp" ^
  /Fo"%NORMALIZE_PATH_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "FilterFiles\FilterFiles.cpp" ^
  /Fo"%FILTER_FILES_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /MP2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" /I "%~dp0." ^
  /c %KERNEL_FILE_HANDLER_SOURCES% ^
  /Fo"%BUILD_OBJ_DIR%/" /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "KernelFileProvider\KernelFileProvider.cpp" ^
  /Fo"%KERNEL_FILE_PROVIDER_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /c "ProcessContext\ProcessContext.cpp" ^
  /Fo"%PROCESS_CONTEXT_OBJ%" ^
  /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /MP2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" /I "%~dp0." ^
  /c %KERNEL_PROCESS_SOURCES% ^
  /Fo"%BUILD_OBJ_DIR%/" /Fd"%PDB%"
if errorlevel 1 exit /b %errorlevel%

link /nologo "%MAIN_OBJ%" %KERNEL_PROCESS_OBJ% "%PROCESS_CONTEXT_OBJ%" "%KERNEL_FILE_PROVIDER_OBJ%" "%HCS_SANDBOX_OBJ%" "%START_PROCESS_OBJ%" "%NORMALIZE_PATH_OBJ%" "%FILTER_FILES_OBJ%" %KERNEL_FILE_HANDLERS_OBJ% ^
  /OUT:"%PROJECT_OUTPUT_EXE%" ^
  tdh.lib advapi32.lib ole32.lib shell32.lib
if errorlevel 1 exit /b %errorlevel%

where go >nul 2>nul
if errorlevel 1 (
  echo Go nije pronadjen u PATH-u; novi HCS sandbox runner nije buildovan.
  exit /b 1
)

pushd "HCS"
if errorlevel 1 exit /b %errorlevel%

go build -buildvcs=false -o "workspace-delete.exe" ".\cmd\workspace-delete"
if errorlevel 1 (
  set "BUILD_RESULT=%errorlevel%"
  popd
  popd
  exit /b %BUILD_RESULT%
)

go build -buildvcs=false -o "sandbox-runner.exe" ".\cmd\sandbox-runner"
set "BUILD_RESULT=%errorlevel%"

popd
popd
exit /b %BUILD_RESULT%
