@echo off
setlocal
cd /d "%~dp0"
if exist "bin\engine_host.exe" (
    start "" "bin\engine_host.exe"
) else (
    echo bin\engine_host.exe not found. Build it first.
)
endlocal
