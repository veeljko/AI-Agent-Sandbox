@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%
set "BUILD_OBJ_DIR=%~dp0build\process-provider-tests\obj"
if not exist "%BUILD_OBJ_DIR%" mkdir "%BUILD_OBJ_DIR%"
call "%~dp0kernel-process-handler-sources.bat"
call "%~dp0kernel-file-handler-sources.bat"
cl /nologo /EHsc /std:c++17 /MP2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
 /I "%~dp0krabs" /c "tests\KernelProcessProviderTests.cpp" %KERNEL_PROCESS_SOURCES% %KERNEL_FILE_HANDLER_SOURCES% ^
 "KernelFileProvider\KernelFileProvider.cpp" "ProcessContext\ProcessContext.cpp" ^
 "NormalizePath\NormalizePath.cpp" "FilterFiles\FilterFiles.cpp" /Fo"%BUILD_OBJ_DIR%/"
if errorlevel 1 exit /b %errorlevel%
link /nologo %KERNEL_PROCESS_OBJ% %KERNEL_FILE_HANDLERS_OBJ% ^
 "%BUILD_OBJ_DIR%\KernelProcessProviderTests.obj" "%BUILD_OBJ_DIR%\KernelFileProvider.obj" ^
 "%BUILD_OBJ_DIR%\ProcessContext.obj" "%BUILD_OBJ_DIR%\NormalizePath.obj" "%BUILD_OBJ_DIR%\FilterFiles.obj" ^
 /OUT:"%~dp0build\process-provider-tests\tests.exe" tdh.lib advapi32.lib ole32.lib shell32.lib
if errorlevel 1 exit /b %errorlevel%
"%~dp0build\process-provider-tests\tests.exe"
set "TEST_RESULT=%errorlevel%"
popd
exit /b %TEST_RESULT%
