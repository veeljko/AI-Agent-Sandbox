@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

set "TEST_TAG=Project3PathPolicy-%RANDOM%-%RANDOM%"
set "TEST_MAIN_OBJ=%TEMP%\%TEST_TAG%-main.obj"
set "TEST_NORMALIZE_OBJ=%TEMP%\%TEST_TAG%-normalize.obj"
set "TEST_FILTER_OBJ=%TEMP%\%TEST_TAG%-filter.obj"
set "TEST_EXE=%TEMP%\%TEST_TAG%.exe"
set "TEST_PDB=%TEMP%\%TEST_TAG%.pdb"

pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0." ^
  /c "tests\PathPolicyTests.cpp" ^
  /Fo"%TEST_MAIN_OBJ%" ^
  /Fd"%TEST_PDB%"
if errorlevel 1 goto :failed

cl /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0." ^
  /c "NormalizePath\NormalizePath.cpp" ^
  /Fo"%TEST_NORMALIZE_OBJ%" ^
  /Fd"%TEST_PDB%"
if errorlevel 1 goto :failed

cl /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0." ^
  /c "FilterFiles\FilterFiles.cpp" ^
  /Fo"%TEST_FILTER_OBJ%" ^
  /Fd"%TEST_PDB%"
if errorlevel 1 goto :failed

link /nologo "%TEST_MAIN_OBJ%" "%TEST_NORMALIZE_OBJ%" "%TEST_FILTER_OBJ%" ^
  /OUT:"%TEST_EXE%" ^
  ole32.lib shell32.lib
if errorlevel 1 goto :failed

"%TEST_EXE%"
set "TEST_RESULT=%errorlevel%"
goto :cleanup

:failed
set "TEST_RESULT=%errorlevel%"

:cleanup
del /q "%TEST_MAIN_OBJ%" "%TEST_NORMALIZE_OBJ%" "%TEST_FILTER_OBJ%" "%TEST_EXE%" "%TEST_PDB%" >nul 2>nul
popd
exit /b %TEST_RESULT%
