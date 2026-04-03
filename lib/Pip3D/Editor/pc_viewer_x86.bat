@echo off
setlocal

REM Check that MSVC compiler (cl.exe) is available
where cl >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: MSVC compiler 'cl.exe' not found in PATH.
    echo Please run this script from "Developer Command Prompt for VS"
    echo or install the "Desktop development with C++" workload in Visual Studio.
    goto :after_run
)

REM Переходим в папку Editor (где лежит этот батник)
cd /d "%~dp0"

REM Сборка редактора
if not exist obj mkdir obj
if not exist bin mkdir bin

echo.
echo Building pc_viewer (compiling sources)...
cl /nologo /EHsc /O2 /DPIP3D_PC ^
    /Fo"obj\\" /c ^
    pc_viewer.cpp ^
    ..\Math\Math.cpp ^
    ..\Core\Core.cpp ^
    ..\Core\Debug\Logging.cpp ^
    ..\Core\Jobs.cpp ^
    ..\Graphics\Font.cpp ^
    ..\Rendering\Rasterizer\Shading.cpp ^
    ..\Rendering\Display\Drivers\PcDisplayDriver.cpp
if errorlevel 1 (
    echo Build failed during compilation.
    goto :after_run
)

echo.
echo Linking pc_viewer.exe (GUI, no console)...
link /nologo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:"bin\pc_viewer.exe" ^
    obj\pc_viewer.obj ^
    obj\Math.obj ^
    obj\Core.obj ^
    obj\Logging.obj ^
    obj\Jobs.obj ^
    obj\Font.obj ^
    obj\Shading.obj ^
    obj\PcDisplayDriver.obj ^
    user32.lib gdi32.lib
if errorlevel 1 (
    echo Build failed during linking.
    goto :after_run
)

REM Запуск exe после успешной сборки (отдельным процессом, чтобы консоль не закрывалась)
if exist "bin\pc_viewer.exe" (
    echo.
    echo Running bin\pc_viewer.exe ...
    start "" "bin\pc_viewer.exe"
) else (
	echo ERROR: bin\pc_viewer.exe not found.
)

:after_run
echo.
echo Press any key to close this window...
pause >nul

endlocal
