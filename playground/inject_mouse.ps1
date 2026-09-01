# PostMessage 注入鼠标/键盘事件到 Luminous_editor_app 窗口（GLFW wndproc 直接处理）。
# 每次调用自动置顶并尝试真实前台聚焦（AttachThreadInput 绕前台锁）。
# 用法: powershell -File inject_mouse.ps1 -mode <mode> [-a n] [-b n]
#   noop                   仅置顶 + 聚焦
#   wheel <notches>        滚轮，正=上（放大/上滚）负=下
#   drag <dx> <dy>         中键从窗口客户区中心按下、位移(dx,dy)物理像素后抬起
#   click <x> <y>          左键单击客户区坐标 (x,y)（GLFW 光标坐标系）
#   keys <vk> <count>      WM_KEYDOWN/UP 虚拟键 vk，重复 count 次（如 backspace 8）
#   text                   通过环境变量 EDIT_TEXT 传入字符串，逐字符 WM_CHAR
param([string]$mode = "", [int]$a = 0, [int]$b = 0)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(IntPtr a, IntPtr b, bool attach);
}
"@

function Focus-Window {
  param([IntPtr]$hwnd)
  $fore = [W]::GetForegroundWindow()
  $fore_thread = [W]::GetWindowThreadProcessId($fore, [IntPtr]::Zero)
  $my_thread = [W]::GetCurrentThreadId()
  [W]::AttachThreadInput($my_thread, $fore_thread, $true) | Out-Null
  [W]::SetForegroundWindow($hwnd) | Out-Null
  [W]::SetFocus($hwnd) | Out-Null
  [W]::AttachThreadInput($my_thread, $fore_thread, $false) | Out-Null
}

$p = Get-Process Luminous_editor_app -ErrorAction Stop
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Error "no main window"; exit 1 }
# HWND_TOPMOST(-1) + NOMOVE(2) + NOSIZE(1) + SHOWWINDOW(0x40)
[W]::SetWindowPos($h, [IntPtr](-1), 0,0,0,0, 3 -bor 0x40) | Out-Null
Focus-Window -hwnd $h
Start-Sleep -Milliseconds 300

$WM_MOUSEMOVE   = 0x0200
$WM_MOUSEWHEEL  = 0x020A
$WM_MBUTTONDOWN = 0x0207
$WM_MBUTTONUP   = 0x0208
$WM_LBUTTONDOWN = 0x0201
$WM_LBUTTONUP   = 0x0202
$WM_KEYDOWN     = 0x0100
$WM_KEYUP       = 0x0101
$WM_CHAR        = 0x0102

# 客户区物理像素中心（960x600 逻辑 × 1.5 缩放 = 1440x900 物理）
$cx = 720; $cy = 450
$lpCenter = [IntPtr](($cy -shl 16) -bor ($cx -band 0xFFFF))

switch ($mode) {
  "noop" { }

  "wheel" {
    $step = if ($a -lt 0) { -120 } else { 120 }
    for ($i = 0; $i -lt [Math]::Abs($a); $i++) {
      [W]::PostMessage($h, $WM_MOUSEWHEEL, [IntPtr]($step -shl 16), $lpCenter) | Out-Null
      Start-Sleep -Milliseconds 60
    }
  }

  "click" {
    $x = $a; $y = $b
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [W]::PostMessage($h, $WM_MOUSEMOVE, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 120
    [W]::PostMessage($h, $WM_LBUTTONDOWN, [IntPtr]1, $lp) | Out-Null
    Start-Sleep -Milliseconds 60
    [W]::PostMessage($h, $WM_LBUTTONUP, [IntPtr]0, $lp) | Out-Null
  }

  "keys" {
    for ($i = 0; $i -lt $b; $i++) {
      [W]::PostMessage($h, $WM_KEYDOWN, [IntPtr]$a, [IntPtr]0) | Out-Null
      Start-Sleep -Milliseconds 30
      [W]::PostMessage($h, $WM_KEYUP, [IntPtr]$a, [IntPtr]0) | Out-Null
      Start-Sleep -Milliseconds 30
    }
  }

  "text" {
    $s = $env:EDIT_TEXT
    foreach ($ch in $s.ToCharArray()) {
      [W]::PostMessage($h, $WM_CHAR, [IntPtr][int]$ch, [IntPtr]0) | Out-Null
      Start-Sleep -Milliseconds 40
    }
  }

  "drag" {
    [W]::PostMessage($h, $WM_MOUSEMOVE, [IntPtr]0, $lpCenter) | Out-Null
    Start-Sleep -Milliseconds 80
    [W]::PostMessage($h, $WM_MBUTTONDOWN, [IntPtr]0, $lpCenter) | Out-Null
    Start-Sleep -Milliseconds 80
    $steps = 10
    for ($i = 1; $i -le $steps; $i++) {
      $x = $cx + [int]($a * $i / $steps)
      $y = $cy + [int]($b * $i / $steps)
      $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
      [W]::PostMessage($h, $WM_MOUSEMOVE, [IntPtr]0, $lp) | Out-Null
      Start-Sleep -Milliseconds 30
    }
    [W]::PostMessage($h, $WM_MBUTTONUP, [IntPtr]0, [IntPtr](($cy + $b -shl 16) -bor (($cx + $a) -band 0xFFFF))) | Out-Null
  }
}
Write-Host "injected: $mode $a $b"
