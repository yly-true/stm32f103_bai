@echo off
rem Configure the portable toolchain. Build separately with tools/build/wt.cmd.
setlocal
where pwsh >nul 2>nul
if errorlevel 1 goto :legacy
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-toolchain.ps1" %*
exit /b %ERRORLEVEL%
:legacy
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-toolchain.ps1" %*
exit /b %ERRORLEVEL%
