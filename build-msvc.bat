@echo off
setlocal

set "PROJECT_INPUT_SOURCE=%~1"
set "PROJECT_OUTPUT_EXE=%~2"
set "PROJECT_OUTPUT_NAME=%~n2"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%

set "MAIN_OBJ=%PROJECT_OUTPUT_NAME%.obj"
set "START_PROCESS_OBJ=StartProcess\StartProcess.obj"
set "NORMALIZE_PATH_OBJ=NormalizePath\NormalizePath.obj"
set "FILTER_FILES_OBJ=FilterFiles\FilterFiles.obj"
set "PDB=%PROJECT_OUTPUT_NAME%.pdb"

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  /I "%~dp0." ^
  /c "%PROJECT_INPUT_SOURCE%" ^
  /Fo"%MAIN_OBJ%" ^
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

link /nologo "%MAIN_OBJ%" "%START_PROCESS_OBJ%" "%NORMALIZE_PATH_OBJ%" "%FILTER_FILES_OBJ%" ^
  /OUT:"%PROJECT_OUTPUT_EXE%" ^
  tdh.lib advapi32.lib ole32.lib shell32.lib

set "BUILD_RESULT=%errorlevel%"
popd
exit /b %BUILD_RESULT%
