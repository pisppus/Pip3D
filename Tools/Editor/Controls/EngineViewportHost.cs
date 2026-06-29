using System.Diagnostics;
using System.Runtime.InteropServices;
using Avalonia.Controls;
using Avalonia.Platform;

namespace Pip3DEditor.Controls;

public sealed class EngineViewportHost : NativeControlHost
{
    private const int WsChild = 0x40000000;
    private const int WsVisible = 0x10000000;
    private IntPtr _hostHwnd;
    private Process? _viewerProcess;

    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        _hostHwnd = CreateWindowEx(
            0,
            "static",
            string.Empty,
            WsChild | WsVisible,
            0,
            0,
            100,
            100,
            parent.Handle,
            IntPtr.Zero,
            IntPtr.Zero,
            IntPtr.Zero);

        StartViewerProcess();
        return new PlatformHandle(_hostHwnd, "HWND");
    }

    protected override void DestroyNativeControlCore(IPlatformHandle control)
    {
        try
        {
            if (_viewerProcess is { HasExited: false })
            {
                _viewerProcess.Kill(true);
                _viewerProcess.WaitForExit(2000);
            }
        }
        catch
        {
        }
        finally
        {
            _viewerProcess?.Dispose();
            _viewerProcess = null;
        }

        if (_hostHwnd != IntPtr.Zero)
        {
            DestroyWindow(_hostHwnd);
            _hostHwnd = IntPtr.Zero;
        }

        base.DestroyNativeControlCore(control);
    }

    private void StartViewerProcess()
    {
        if (_hostHwnd == IntPtr.Zero || _viewerProcess is { HasExited: false })
        {
            return;
        }

        var repoRoot = FindRepoRoot();
        if (repoRoot is null)
        {
            return;
        }

        var exePath = Path.Combine(repoRoot, "tools", "Pip3DEditor", "Native", "bin", "engine_host.exe");
        var scenePath = Path.Combine(repoRoot, "tools", "Pip3DEditor", "live_scene.snapshot");
        if (!File.Exists(exePath))
        {
            return;
        }

        var handleText = $"0x{_hostHwnd.ToInt64():X}";
        var startInfo = new ProcessStartInfo
        {
            FileName = exePath,
            Arguments = $"--hosted {handleText} --scene \"{scenePath}\"",
            WorkingDirectory = Path.GetDirectoryName(exePath) ?? repoRoot,
            UseShellExecute = false
        };

        _viewerProcess = Process.Start(startInfo);
    }

    private static string? FindRepoRoot()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);
        while (current is not null)
        {
            var candidate = Path.Combine(current.FullName, "platformio.ini");
            if (File.Exists(candidate))
            {
                return current.FullName;
            }

            current = current.Parent;
        }

        return null;
    }

    [DllImport("user32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr CreateWindowEx(
        int dwExStyle,
        string lpClassName,
        string lpWindowName,
        int dwStyle,
        int x,
        int y,
        int nWidth,
        int nHeight,
        IntPtr hWndParent,
        IntPtr hMenu,
        IntPtr hInstance,
        IntPtr lpParam);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyWindow(IntPtr hWnd);
}
