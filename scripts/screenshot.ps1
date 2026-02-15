Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# P/Invoke to get window rect and enumerate windows
Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    using System.Text;
    public class Win32 {
        [DllImport("user32.dll")]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }
    }
"@

# Find window by searching for title containing "PaperMarioPC"
$script:foundHwnd = [IntPtr]::Zero
$callback = {
    param($hwnd, $lParam)
    $sb = New-Object System.Text.StringBuilder(256)
    [Win32]::GetWindowText($hwnd, $sb, $sb.Capacity) | Out-Null
    $title = $sb.ToString()
    if ($title -like "*Paper Mario*") {
        $script:foundHwnd = $hwnd
        return $false  # Stop enumeration
    }
    return $true  # Continue enumeration
}

[Win32]::EnumWindows($callback, [IntPtr]::Zero) | Out-Null
$hwnd = $script:foundHwnd

if ($hwnd -eq [IntPtr]::Zero) {
    Write-Error "Paper Mario window not found. Is the game running?"
    exit 1
}

# Get window bounds
$rect = New-Object Win32+RECT
if (-not [Win32]::GetWindowRect($hwnd, [ref]$rect)) {
    Write-Error "Failed to get window bounds"
    exit 1
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top

if ($width -le 0 -or $height -le 0) {
    Write-Error "Invalid window dimensions: ${width}x${height}"
    exit 1
}

# Capture just the game window
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, [System.Drawing.Size]::new($width, $height))
$graphics.Dispose()

$outPath = Join-Path $PSScriptRoot "..\screenshot.png"
$bitmap.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

Write-Host "Screenshot saved to $outPath (${width}x${height})"
