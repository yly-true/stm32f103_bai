@echo off
rem WHEELTEC firmware build entry (cmd / double-click).
rem Usage is the same as wt.ps1, e.g.:  wt -Mode Release -Flash
setlocal
where pwsh >nul 2>nul
if errorlevel 1 goto :use_legacy
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build\wt.ps1" %*
goto :done
:use_legacy
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build\wt.ps1" %*
:done
exit /b %ERRORLEVEL%