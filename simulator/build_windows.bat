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

echo Building Pip3D desktop simulator...
cl /nologo /EHsc /O2 /std:c++17 ^
    /DPIP3D_PC ^
    /DPIP3D_SCREEN_WIDTH=1280 ^
    /DPIP3D_SCREEN_HEIGHT=720 ^
    /DPIP3D_SCREEN_BAND_COUNT=1 ^
    /DPIPGUI_SIM_SCALE=1 ^
    /I".\include" ^
    /I"..\lib\Pip3D" ^
    /I"..\lib\Pip3D\Pip3D" ^
    /I"..\include" ^
    /Fo"obj\\" /c ^
    src\Runner.cpp ^
    src\ArduinoShim.cpp ^
    src\Pip3DDesktopBlit.cpp ^
    ..\src\main.cpp ^
    ..\lib\Pip3D\Pip3D\Math\Math.cpp ^
    ..\lib\Pip3D\Pip3D\Core\Core.cpp ^
    ..\lib\Pip3D\Pip3D\Core\Debug\Logging.cpp ^
    ..\lib\Pip3D\Pip3D\Core\Jobs.cpp ^
    ..\lib\Pip3D\Pip3D\Graphics\Font.cpp ^
    ..\lib\Pip3D\Pip3D\Rendering\Rasterizer\Shading.cpp ^
    ..\lib\Pip3D\PipCore\Platforms\Desktop\Runtime.cpp
if errorlevel 1 exit /b 1

echo Linking simulator executable...
link /nologo /SUBSYSTEM:WINDOWS /OUT:"bin\pip3d_simulator.exe" ^
    obj\Runner.obj ^
    obj\ArduinoShim.obj ^
    obj\Pip3DDesktopBlit.obj ^
    obj\main.obj ^
    obj\Math.obj ^
    obj\Core.obj ^
    obj\Logging.obj ^
    obj\Jobs.obj ^
    obj\Font.obj ^
    obj\Shading.obj ^
    obj\Runtime.obj ^
    user32.lib ^
    gdi32.lib ^
    mfplat.lib ^
    mfreadwrite.lib ^
    mfuuid.lib ^
    ole32.lib ^
    windowscodecs.lib
if errorlevel 1 exit /b 1

start "" "bin\pip3d_simulator.exe"
