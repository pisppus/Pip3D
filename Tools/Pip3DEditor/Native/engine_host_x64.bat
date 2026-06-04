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
    echo.
    echo ERROR: MSVC compiler 'cl.exe' not found in PATH.
    echo Install Visual Studio C++ tools or run from Developer Command Prompt.
    goto :after_run
)

cd /d "%~dp0"

if not exist obj mkdir obj
if not exist bin mkdir bin

echo.
echo Building engine_host...
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
if errorlevel 1 (
    echo Build failed during compilation.
    goto :after_run
)

echo.
echo Linking engine_host.exe...
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
if errorlevel 1 (
    echo Build failed during linking.
    goto :after_run
)

if exist "bin\engine_host.exe" (
    echo.
    echo Running bin\engine_host.exe ...
    start "" "bin\engine_host.exe"
) else (
    echo ERROR: bin\engine_host.exe not found.
)

:after_run
echo.
echo Press any key to close this window...
pause >nul

endlocal
