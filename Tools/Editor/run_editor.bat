@echo off
setlocal
cd /d "%~dp0"
call "Native\\build_engine_host.bat"
if errorlevel 1 exit /b 1
dotnet run -c Release
