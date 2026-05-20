@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

cl /nologo /EHsc /std:c++17 /Zi /FS /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
  /I "%~dp0krabs" ^
  "%~1" ^
  /Fo:"%~dpn2.obj" ^
  /Fd:"%~dpn2.pdb" ^
  /Fe:"%~2" ^
  /link tdh.lib advapi32.lib ole32.lib

exit /b %errorlevel%
