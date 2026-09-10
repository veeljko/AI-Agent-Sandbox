@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

set "TEST_TAG=Project3ProcessContext-%RANDOM%-%RANDOM%"
set "TEST_MAIN_OBJ=%TEMP%\%TEST_TAG%-main.obj"
set "TEST_CONTEXT_OBJ=%TEMP%\%TEST_TAG%-context.obj"
set "TEST_EXE=%TEMP%\%TEST_TAG%.exe"

cl /nologo /EHsc /std:c++17 /c "%~dp0tests\ProcessContextTests.cpp" /Fo"%TEST_MAIN_OBJ%"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /c "%~dp0ProcessContext\ProcessContext.cpp" /Fo"%TEST_CONTEXT_OBJ%"
if errorlevel 1 exit /b %errorlevel%

link /nologo "%TEST_MAIN_OBJ%" "%TEST_CONTEXT_OBJ%" /OUT:"%TEST_EXE%"
if errorlevel 1 exit /b %errorlevel%

"%TEST_EXE%"
exit /b %errorlevel%
