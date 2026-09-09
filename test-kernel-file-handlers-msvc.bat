@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
set "TEST_DIR=%TEMP%\Project3KernelHandlers-%RANDOM%-%RANDOM%"
mkdir "%TEST_DIR%"
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%
call "%~dp0kernel-file-handler-sources.bat"
cl /nologo /EHsc /std:c++17 /MP2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" /c "tests\KernelFileHandlersTests.cpp" %KERNEL_FILE_HANDLER_SOURCES% ^
  "KernelFileProvider\KernelFileProvider.cpp" ^
  "NormalizePath\NormalizePath.cpp" "FilterFiles\FilterFiles.cpp" "StartProcess\StartProcess.cpp" ^
  /Fo"%TEST_DIR%/"
if errorlevel 1 exit /b %errorlevel%
link /nologo "%TEST_DIR%\*.obj" /OUT:"%TEST_DIR%\tests.exe" tdh.lib advapi32.lib ole32.lib shell32.lib
if errorlevel 1 exit /b %errorlevel%
"%TEST_DIR%\tests.exe"
set "TEST_RESULT=%errorlevel%"
popd
exit /b %TEST_RESULT%
