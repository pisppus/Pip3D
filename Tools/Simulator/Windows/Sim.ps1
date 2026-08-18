#Requires -Version 5.1
param(
    [switch]$NoRun,
    [switch]$Debug,
    [switch]$Clean,
    [switch]$InstallDeps,
    [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot([string]$startDir) {
    $dir = (Resolve-Path $startDir).Path
    while ($true) {
        $hasPlatformio = Test-Path (Join-Path $dir "platformio.ini")
        $hasInclude    = Test-Path (Join-Path $dir "include")
        $hasLib        = Test-Path (Join-Path $dir "lib\Pip3D")
        $hasTools      = Test-Path (Join-Path $dir "Tools\Simulator")
        if ($hasPlatformio -or ($hasInclude -and $hasLib -and $hasTools)) {
            return $dir
        }

        $parent = Split-Path -Parent $dir
        if ([string]::IsNullOrEmpty($parent) -or $parent -eq $dir) {
            break
        }
        $dir = $parent
    }

    return (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
}

$root           = Resolve-ProjectRoot $PSScriptRoot
$buildDir       = Join-Path $root ".sim"
$configName     = if ($Debug) { "debug" } else { "release" }
$cmakeConfig    = "Release"
$simDebug       = if ($Debug) { "ON" } else { "OFF" }
$cmakeBuildDir  = Join-Path $buildDir "build-windows-$configName"
$vcpkgDir       = Join-Path $buildDir "deps\vcpkg"
$vcpkgTriplet   = "x64-windows-release"
$vcpkgInstalled = Join-Path $vcpkgDir "installed\$vcpkgTriplet"
$vsEnvFile      = Join-Path $buildDir "_vsdev-env-$PID.txt"
$vsEnvCacheFile = Join-Path $buildDir "_vsdev-env-cache.txt"
$vsEnvStampFile = Join-Path $buildDir "_vsdev-env-cache.stamp"
$watchdogFile   = Join-Path $buildDir "_sim-watchdog.ps1"
$watchdogLog    = Join-Path $buildDir "sim-watchdog.log"
$runtimeOutLog  = Join-Path $buildDir "simulator-runtime.out.log"
$runtimeErrLog  = Join-Path $buildDir "simulator-runtime.err.log"
$exeName        = if ($Debug) { "simulator-debug.exe" } else { "simulator.exe" }
$exe            = Join-Path $buildDir $exeName

function Write-Step  ([string]$msg) { Write-Host "  >> $msg" -ForegroundColor Cyan }
function Write-Ok    ([string]$msg) { Write-Host "  OK $msg" -ForegroundColor Green }
function Write-Warn  ([string]$msg) { Write-Host "  !! $msg" -ForegroundColor Yellow }
function Write-Fatal ([string]$msg) { Write-Host "`n  FAIL $msg`n" -ForegroundColor Red; exit 1 }

function ConvertTo-CMakePath([string]$path) {
    return ($path -replace '\\', '/')
}

function Test-MsvcIncludeNote([string]$line) {
    return $line -match '^\s*Note:\s+including file:' -or
           $line -match '^\s*[^:]+:\s+[^:]+:\s+[A-Za-z]:\\' -or
           $line -match '^\s*[^:]{4,100}:\s+[A-Za-z]:\\.*\.(h|hh|hpp|hxx|inl|inc)\s*$'
}

function Invoke-CMakeBuildFiltered([string]$buildPath, [string]$config, [int]$parallelJobs) {
    & cmake.exe --build $buildPath --config $config --parallel $parallelJobs 2>&1 |
        ForEach-Object {
            $line = "$_"
            if (!(Test-MsvcIncludeNote $line)) {
                Write-Host $line
            }
        }
    return $LASTEXITCODE
}

function Invoke-CMakeConfigureWithRetry([string[]]$cmakeArgs, [string]$buildPath) {
    $quotedArgs = foreach ($arg in $cmakeArgs) {
        if ($arg -match '\s') { '"' + $arg + '"' } else { $arg }
    }
    $cmdFile = Join-Path $buildDir "_cmake-configure.cmd"
    $outFile = Join-Path $buildDir "_cmake-configure.out.txt"
    $errFile = Join-Path $buildDir "_cmake-configure.err.txt"
    $cmdLines = [System.Collections.Generic.List[string]]@(
        "@echo off",
        ('call "' + $vcvars + '" >nul 2>&1'),
        ('cmake.exe ' + ($quotedArgs -join ' '))
    )
    [IO.File]::WriteAllLines($cmdFile, $cmdLines, [Text.Encoding]::ASCII)
    foreach ($path in @($outFile, $errFile)) {
        if (Test-Path $path) { Remove-Item $path -Force -ErrorAction SilentlyContinue }
    }
    $proc = Start-Process -FilePath cmd.exe `
                          -ArgumentList @("/d", "/s", "/c", "`"$cmdFile`"") `
                          -WorkingDirectory $buildDir `
                          -PassThru `
                          -NoNewWindow `
                          -Wait `
                          -RedirectStandardOutput $outFile `
                          -RedirectStandardError $errFile
    if (Test-Path $outFile) {
        foreach ($line in [IO.File]::ReadAllLines($outFile)) { if ($line) { Write-Host $line } }
    }
    if (Test-Path $errFile) {
        foreach ($line in [IO.File]::ReadAllLines($errFile)) { if ($line) { Write-Host $line -ForegroundColor DarkRed } }
    }
    $exitCode = $proc.ExitCode
    if ($exitCode -eq 0) {
        Remove-Item $cmdFile, $outFile, $errFile -Force -ErrorAction SilentlyContinue
        return 0
    }

    $firstExitCode = $exitCode
    if (Test-Path $buildPath) {
        Write-Warn "CMake configure failed; clearing stale build tree and retrying once."
        Remove-Item $buildPath -Recurse -Force
        New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
        [IO.File]::WriteAllLines($cmdFile, $cmdLines, [Text.Encoding]::ASCII)
        foreach ($path in @($outFile, $errFile)) {
            if (Test-Path $path) { Remove-Item $path -Force -ErrorAction SilentlyContinue }
        }
        $proc = Start-Process -FilePath cmd.exe `
                              -ArgumentList @("/d", "/s", "/c", "`"$cmdFile`"") `
                              -WorkingDirectory $buildDir `
                              -PassThru `
                              -NoNewWindow `
                              -Wait `
                              -RedirectStandardOutput $outFile `
                              -RedirectStandardError $errFile
        if (Test-Path $outFile) {
            foreach ($line in [IO.File]::ReadAllLines($outFile)) { if ($line) { Write-Host $line } }
        }
        if (Test-Path $errFile) {
            foreach ($line in [IO.File]::ReadAllLines($errFile)) { if ($line) { Write-Host $line -ForegroundColor DarkRed } }
        }
        $retryExitCode = $proc.ExitCode
        if ($retryExitCode -eq 0) {
            Remove-Item $cmdFile, $outFile, $errFile -Force -ErrorAction SilentlyContinue
        }
        return $retryExitCode
    }

    return $firstExitCode
}

function Reset-CMakeBuildIfMoved([string]$buildPath, [string]$sourcePath, [string]$rootPath) {
    $cache = Join-Path $buildPath "CMakeCache.txt"
    if (!(Test-Path $cache)) { return }

    $expectedSource = (ConvertTo-CMakePath (Resolve-Path $sourcePath).Path).ToLowerInvariant()
    $expectedRoot = (ConvertTo-CMakePath (Resolve-Path $rootPath).Path).ToLowerInvariant()
    $actualSource = ""
    $actualRoot = ""

    foreach ($line in [IO.File]::ReadLines($cache)) {
        if ($line -match '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$') {
            $actualSource = ($Matches[1] -replace '\\', '/').ToLowerInvariant()
        } elseif ($line -match '^SIM_ROOT:[^=]+=([^=].*)$') {
            $actualRoot = ($Matches[1] -replace '\\', '/').ToLowerInvariant()
        }
    }

    if (($actualSource -and $actualSource -ne $expectedSource) -or
        ($actualRoot -and $actualRoot -ne $expectedRoot)) {
        Write-Warn "CMake cache belongs to another checkout - rebuilding simulator cache."
        Remove-Item $buildPath -Recurse -Force
        New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
    }
}

function Resolve-VcVars64 {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPaths = @(
            & $vswhere -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath 2>$null
            & $vswhere -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath 2>$null
        ) | Where-Object { $_ } | Select-Object -Unique

        foreach ($ip in $installPaths) {
            $bat = Join-Path $ip "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $bat) { return $bat }
        }
    }

    foreach ($searchRoot in @(
        "C:\Program Files\Microsoft Visual Studio",
        "C:\Program Files (x86)\Microsoft Visual Studio"
    )) {
        if (!(Test-Path $searchRoot)) { continue }
        $bat = Get-ChildItem $searchRoot -Recurse -Filter "vcvars64.bat" -ErrorAction SilentlyContinue |
               Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
        if ($bat) { return $bat }
    }
    return $null
}

function Import-VsEnvironment([string]$vcvars) {
    $cleanPath = (@(
        [Environment]::GetEnvironmentVariable("Path", "Machine"),
        [Environment]::GetEnvironmentVariable("Path", "User")
    ) | Where-Object { $_ } | ForEach-Object { $_.Trim(';') }) -join ';'

    $vcvarsItem = Get-Item $vcvars
    $pathHash = ([Security.Cryptography.SHA256]::Create().ComputeHash(
                    [Text.Encoding]::UTF8.GetBytes($cleanPath)) |
                 ForEach-Object { $_.ToString("x2") }) -join ""
    $stamp = "$vcvars|$($vcvarsItem.LastWriteTimeUtc.Ticks)|$pathHash"
    $cachedStamp = if (Test-Path $vsEnvStampFile) { Get-Content $vsEnvStampFile -Raw } else { "" }

    if ((Test-Path $vsEnvCacheFile) -and ($cachedStamp.Trim() -eq $stamp)) {
        Write-Ok "Using cached VS environment."
        Copy-Item $vsEnvCacheFile $vsEnvFile -Force
    } else {
        $cmdFile = Join-Path $buildDir "_vsdev-bootstrap.cmd"
        $varsToClear = @(
            "INCLUDE","LIB","LIBPATH","DevEnvDir","ExtensionSdkDir",
            "Framework40Version","FrameworkDir","FrameworkDir32",
            "FrameworkVersion","FrameworkVersion32","FrameworkVersion64",
            "UCRTVersion","UniversalCRTSdkDir","VCIDEInstallDir","VCINSTALLDIR",
            "VCToolsInstallDir","VSINSTALLDIR","VisualStudioVersion",
            "WindowsLibPath","WindowsSdkBinPath","WindowsSdkDir",
            "WindowsSdkVerBinPath","WindowsSDKLibVersion","WindowsSDKVersion",
            "__VSCMD_PREINIT_PATH","__VSCMD_ARG_APP_PLAT","__VSCMD_ARG_HOST_ARCH",
            "__VSCMD_ARG_NO_LOGO","__VSCMD_ARG_TGT_ARCH","__VSCMD_VER"
        )

        $lines = [Collections.Generic.List[string]]@(
            "@echo off",
            "setlocal EnableExtensions",
            ('set "PATH=' + ($cleanPath -replace '"', '') + '"')
        )
        foreach ($v in $varsToClear) { $lines.Add("set `"$v=`"") }
        $lines.Add("call `"$vcvars`" >nul 2>&1")
        $lines.Add("if %errorlevel% neq 0 exit /b %errorlevel%")
        $lines.Add("set > `"$vsEnvFile`"")

        [IO.File]::WriteAllLines($cmdFile, $lines, [Text.Encoding]::ASCII)
        $null = & cmd.exe /d /s /c "`"$cmdFile`""
        if ($LASTEXITCODE -ne 0 -or !(Test-Path $vsEnvFile)) {
            Write-Fatal "Failed to initialize VS build environment (exit $LASTEXITCODE)."
        }
        Copy-Item $vsEnvFile $vsEnvCacheFile -Force
        Set-Content -Path $vsEnvStampFile -Value $stamp -NoNewline
        Remove-Item $cmdFile -Force -ErrorAction SilentlyContinue
    }

    foreach ($line in [IO.File]::ReadAllLines($vsEnvFile)) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    Remove-Item $vsEnvFile -Force -ErrorAction SilentlyContinue
}

$buildStart = [Diagnostics.Stopwatch]::StartNew()
Write-Host ""
Write-Host "  simulator" -ForegroundColor DarkGray
Write-Host "  $configName  jobs=$Jobs  root=$root" -ForegroundColor DarkGray
Write-Host ""

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

if ($Clean) {
    Write-Step "Cleaning build outputs..."
    if (Test-Path $cmakeBuildDir) { 
        cmd.exe /c "rd /s /q `"\`?\`?$cmakeBuildDir`"" 2>$null
        if (Test-Path $cmakeBuildDir) { Remove-Item $cmakeBuildDir -Recurse -Force -ErrorAction SilentlyContinue }
    }
    if (Test-Path $exe) { Remove-Item $exe -Force }
    foreach ($stale in @(
        "pipgui-sim.exe", "pipgui-sim-debug.exe", "pipgui-sim-debug.pdb", "pipgui-sim-debug.ilk",
        "cl-args.rsp", "gxx-args.rsp", "link-args.rsp", "flags.hash", "flags-release.hash",
        "gxx.err", "gxx.out", "probe.cpp", "probe.err", "probe.out", "Runtime.test.obj", "temp.obj",
        "sim-build.err.log", "sim-build.out.log", "_sim-watchdog.ps1", "sim-watchdog.log",
        "simulator-runtime.log", "simulator-runtime.out.log", "simulator-runtime.err.log",
        "_cmake-configure.cmd", "_cmake-configure.out.txt", "_cmake-configure.err.txt"
    )) {
        $path = Join-Path $buildDir $stale
        if (Test-Path $path) { Remove-Item $path -Force -ErrorAction SilentlyContinue }
    }
    foreach ($staleDir in @("obj", "obj-release", "cmake-debug", "cmake-release")) {
        $path = Join-Path $buildDir $staleDir
        if (Test-Path $path) { Remove-Item $path -Recurse -Force -ErrorAction SilentlyContinue }
    }
    Write-Ok "Clean done."
}

if ($InstallDeps) {
    & (Join-Path $PSScriptRoot "Sim-Deps.ps1") -EnsureBuildTools
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Step "Locating Visual Studio C++ tools..."
$vcvars = Resolve-VcVars64
if (!$vcvars -or !(Test-Path $vcvars)) {
    Write-Fatal "vcvars64.bat not found. Run Tools\Sim.ps1 -InstallDeps or install Visual Studio C++ build tools."
}
Write-Ok "Found: $vcvars"

Write-Step "Initializing VS build environment..."
Import-VsEnvironment $vcvars

$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
$link = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
if (!$cl -or !$link) { Write-Fatal "cl.exe / link.exe not found after VS env init." }
Write-Ok "Compiler : $cl"
Write-Ok "Linker   : $link"

$wxHeader = Join-Path $vcpkgInstalled "include\wx\wx.h"
if (!(Test-Path $wxHeader)) {
    & (Join-Path $PSScriptRoot "Sim-Deps.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (!(Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
    Write-Fatal "cmake.exe not found. Run Tools\Sim.ps1 -InstallDeps or install CMake."
}

$exeProcessName = [IO.Path]::GetFileNameWithoutExtension($exe)
$watchdogs = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
             Where-Object {
                 $_.ProcessId -ne $PID -and
                 $_.CommandLine -and
                 $_.CommandLine.IndexOf($watchdogFile, [StringComparison]::OrdinalIgnoreCase) -ge 0
             }
if ($watchdogs) {
    Write-Warn "Terminating simulator watchdog..."
    foreach ($watchdog in $watchdogs) {
        try { Stop-Process -Id $watchdog.ProcessId -Force -ErrorAction Stop } catch {}
    }
}
$running = Get-Process -Name $exeProcessName -ErrorAction SilentlyContinue |
           Where-Object {
               try {
                   $_.Path -and [StringComparer]::OrdinalIgnoreCase.Equals($_.Path, $exe)
               } catch {
                   $false
               }
           }
if ($running) {
    Write-Warn "Terminating running simulator (PID $($running.Id))..."
    $running | Stop-Process -Force
    try { $running | Wait-Process -Timeout 10 -ErrorAction Stop } catch {}
    Write-Ok "Simulator stopped."
}

$toolchainFile = Join-Path $vcpkgDir "scripts\buildsystems\vcpkg.cmake"
$ninjaCmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
$ninjaPath = if ($ninjaCmd) { $ninjaCmd.Source } else {
    $vsRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $vcvars)))
    $vsNinja = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (Test-Path $vsNinja) {
        $vsNinja
    } else {
        Get-ChildItem (Join-Path $vcpkgDir "downloads\tools\ninja") -Recurse -Filter "ninja.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }
}

$env:Path = (Join-Path $vcpkgInstalled "bin") + ";" + (Join-Path $vcpkgInstalled "debug\bin") + ";" + $env:Path
$env:VSLANG = "1033"

$cmakeArgs = @(
    "-S", (Join-Path $root "Tools\Simulator"),
    "-B", $cmakeBuildDir,
    "--toolchain", $toolchainFile,
    "-DSIM_ROOT=$root",
    "-DSIM_DEBUG=$simDebug",
    "-DVCPKG_TARGET_TRIPLET=$vcpkgTriplet",
    "-DCMAKE_BUILD_TYPE=$cmakeConfig",
    "-DCMAKE_CXX_COMPILER=$cl"
)
if ($ninjaPath) {
    $cmakeArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$(ConvertTo-CMakePath $ninjaPath)")
}

Reset-CMakeBuildIfMoved $cmakeBuildDir (Join-Path $root "Tools\Simulator") $root

Write-Step "Configuring wxWidgets simulator..."
$configureExitCode = Invoke-CMakeConfigureWithRetry $cmakeArgs $cmakeBuildDir
if ($configureExitCode -ne 0) { Write-Fatal "CMake configure failed." }

Write-Step "Building wxWidgets simulator..."
$buildExitCode = Invoke-CMakeBuildFiltered $cmakeBuildDir $cmakeConfig $Jobs
if ($buildExitCode -ne 0) { Write-Fatal "CMake build failed." }

$buildStart.Stop()
$elapsed = $buildStart.Elapsed
Write-Host ""
Write-Host "  done in $($elapsed.ToString('mm\:ss\.f'))  ->  $exe" -ForegroundColor DarkGray
Write-Host ""

if (!$NoRun) {
    Write-Host "  launching..." -ForegroundColor DarkGray
    $runtimePath = (Join-Path $vcpkgInstalled "bin") + ";" + (Join-Path $vcpkgInstalled "debug\bin") + ";" + $env:Path
    $watchdogLines = [string[]]@(
        '$ErrorActionPreference = "Stop"',
        ('$exe = ' + "'" + ($exe -replace "'", "''") + "'"),
        ('$wd = ' + "'" + ($buildDir -replace "'", "''") + "'"),
        ('$log = ' + "'" + ($watchdogLog -replace "'", "''") + "'"),
        ('$runtimeOutLog = ' + "'" + ($runtimeOutLog -replace "'", "''") + "'"),
        ('$runtimeErrLog = ' + "'" + ($runtimeErrLog -replace "'", "''") + "'"),
        ('$env:Path = ' + "'" + ($runtimePath -replace "'", "''") + "'"),
        ('$env:PIPGUI_SIM_WORKDIR = ' + "'" + ($buildDir -replace "'", "''") + "'"),
        '$env:PIPGUI_SIM_THEME = ""',
        '$env:PIPGUI_SIM_LAST_EXIT = ""',
        '$userExit = Join-Path $wd "_sim-user-exit"',
        '$restart = Join-Path $wd "_sim-restart"',
        'Remove-Item $userExit, $restart -Force -ErrorAction SilentlyContinue',
        'while ($true) {',
        '    $p = Start-Process -FilePath $exe -WorkingDirectory $wd -WindowStyle Normal -RedirectStandardOutput $runtimeOutLog -RedirectStandardError $runtimeErrLog -PassThru -Wait',
        '    if (Test-Path $userExit) { Remove-Item $userExit -Force -ErrorAction SilentlyContinue; break }',
        '    if (Test-Path $restart) { Remove-Item $restart -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 150; continue }',
        '    if ($p.ExitCode -eq 0) { break }',
        '    $msg = "previous simulator exited with code {0}; restarted at {1:yyyy-MM-dd HH:mm:ss}" -f $p.ExitCode, (Get-Date)',
        '    Add-Content -Path $log -Value $msg',
        '    $env:PIPGUI_SIM_LAST_EXIT = $msg',
        '    Start-Sleep -Milliseconds 500',
        '}'
    )
    [IO.File]::WriteAllLines($watchdogFile, $watchdogLines, [Text.Encoding]::ASCII)
    Start-Process -FilePath "powershell.exe" `
                  -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $watchdogFile) `
                  -WorkingDirectory $buildDir `
                  -WindowStyle Hidden | Out-Null
}
