@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%~dp0"
if errorlevel 1 exit /b 1
if not exist "..\..\build\dll-injection-test" mkdir "..\..\build\dll-injection-test"
cl /nologo /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /LD TestPayload.cpp /Fo"..\..\build\dll-injection-test\TestPayload.obj" /Fe"..\..\build\dll-injection-test\TestPayload.dll" /link /INCREMENTAL:NO
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN DllInjectionTest.cpp /Fo"..\..\build\dll-injection-test\DllInjectionTest.obj" /Fe"..\..\build\dll-injection-test\DllInjectionTest.exe" /link /INCREMENTAL:NO
if errorlevel 1 goto failed
"..\..\build\dll-injection-test\DllInjectionTest.exe"
set "TEST_RESULT=%errorlevel%"
popd
exit /b %TEST_RESULT%
:failed
popd
exit /b 1
