# Resizes the MediaCurator main window to an exact client-area size for consistent screenshots.
# Usage: .\resize-window.ps1 [-Width 1440] [-Height 900] [-ProcessName "MediaCurator"]

param(
	[int]$Width = 1440,
	[int]$Height = 900,
	[string]$ProcessName = "MediaCurator"
)

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class Win32 {
	[DllImport("user32.dll")]
	public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

	[DllImport("user32.dll")]
	public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

	[DllImport("user32.dll")]
	public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

	[StructLayout(LayoutKind.Sequential)]
	public struct RECT {
		public int Left;
		public int Top;
		public int Right;
		public int Bottom;
	}
}
"@

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1

if (-not $proc) {
	Write-Error "No running process named '$ProcessName' with a visible window found. Is MediaCurator running (not just open in Visual Studio)?"
	exit 1
}

$hwnd = $proc.MainWindowHandle

$outerRect = New-Object Win32+RECT
$clientRect = New-Object Win32+RECT
[Win32]::GetWindowRect($hwnd, [ref]$outerRect) | Out-Null
[Win32]::GetClientRect($hwnd, [ref]$clientRect) | Out-Null

$borderWidth = ($outerRect.Right - $outerRect.Left) - ($clientRect.Right - $clientRect.Left)
$borderHeight = ($outerRect.Bottom - $outerRect.Top) - ($clientRect.Bottom - $clientRect.Top)

$targetOuterWidth = $Width + $borderWidth
$targetOuterHeight = $Height + $borderHeight

# SWP_NOZORDER = 0x0004, SWP_NOACTIVATE = 0x0010
$SWP_NOZORDER = 0x0004
$SWP_NOACTIVATE = 0x0010

[Win32]::SetWindowPos($hwnd, [IntPtr]::Zero, $outerRect.Left, $outerRect.Top, $targetOuterWidth, $targetOuterHeight, ($SWP_NOZORDER -bor $SWP_NOACTIVATE)) | Out-Null

Write-Host "Resized '$($proc.MainWindowTitle)' client area to ${Width}x${Height} (outer: ${targetOuterWidth}x${targetOuterHeight})"
