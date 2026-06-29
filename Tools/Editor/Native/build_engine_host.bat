@echo off
setlocal

where cl >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: MSVC compiler 'cl.exe' not found in PATH.
    exit /b 1
)

cd /d "%~dp0"

if not exist obj mkdir obj
if not exist bin mkdir bin

cl /nologo /EHsc /O2 /std:c++17 /DPIP3D_PC /DPIP3D_SCREEN_WIDTH=960 /DPIP3D_SCREEN_HEIGHT=540 /DPIP3D_SCREEN_BAND_COUNT=4 ^
    /I"..\..\..\lib\Pip3D" ^
    /I"..\..\..\lib\Pip3D\Pip3D" ^
    /I"..\..\..\include" ^
    /Fo"obj\\" /c ^
    engine_host.cpp ^
    PcDisplayBlit.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Math\Math.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Core\Core.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Core\Debug\Logging.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Core\Jobs.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Graphics\Font.cpp ^
    ..\..\..\lib\Pip3D\Pip3D\Rendering\Rasterizer\Shading.cpp
if errorlevel 1 exit /b 1

link /nologo /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:"bin\engine_host.exe" ^
    obj\engine_host.obj ^
    obj\Math.obj ^
    obj\Core.obj ^
    obj\Logging.obj ^
    obj\Jobs.obj ^
    obj\Font.obj ^
    obj\Shading.obj ^
    obj\PcDisplayBlit.obj ^
    user32.lib gdi32.lib
if errorlevel 1 exit /b 1

exit /b 0
