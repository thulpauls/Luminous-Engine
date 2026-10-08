# check-glimmer-boundary.ps1 —— 机械校验 glimmer 基础平台与引擎高层之间的依赖方向
#
# 分层（CMakeLists.txt 里的三个目标也是这一条链）：
#     glimmer（基础平台：数学 + 平台 + 渲染器）  →  Luminous（引擎高层）
#
# 这个脚本查【包含关系】与【链接行】，两者合起来才算边界被钉住：
#   规则 1：glimmer 源码/公共头只允许包含自己的头、third_party 的 glad/stb/GLFW、以及标准库。
#           一旦 glimmer 直接包含 application/assets/scene/ecs/serial/ui/effects/components… 就报错。
#   规则 2：glimmer 的公共头不允许包含任何 third_party 头（消费者不该被迫看到 glad/stb/GLFW）。
#   规则 3：所有 `glimmer*` 目标（glimmer_hello / glimmer_hello3d / glimmer_check …）
#           的链接行里不允许出现 Luminous —— 这是"glimmer 能独立运行"的直接判据；
#           构建产物不存在时跳过（不算失败）。新增 glimmer* 目标会自动进入检查范围。
#
# 用法：& .\tools\check-glimmer-boundary.ps1      （有违规时退出码为 1）
# 注意：本文件含中文，必须以 **UTF-8 带 BOM** 保存 —— 无 BOM 时 Windows PowerShell
#       会按 CP936 解码并直接语法报错。改动请用 .NET UTF8Encoding($true) 写回。

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$violations = New-Object System.Collections.Generic.List[string]

$ownPrefix = 'glimmer/'
$allowedThirdParty = @('glad/glad.h', 'stb_image.h', 'stb_truetype.h', 'GLFW/glfw3.h')

function Get-LumIncludes([string]$path) {
  Select-String -Path $path -Pattern '^\s*#\s*include\s+[<"]([^>"]+)[>"]' -AllMatches |
    ForEach-Object { $_.Matches | ForEach-Object { $_.Groups[1].Value } }
}

function Test-Allowed([string]$inc) {
  if ($inc.StartsWith($ownPrefix)) { return $true }
  if ($allowedThirdParty -contains $inc) { return $true }
  if ($inc -match '^[a-z_0-9]+\.h$' -and -not $inc.StartsWith('lum_')) { return $true }   # 标准库
  if ($inc -match '^(sys/|GL/|KHR/)') { return $true }
  return $false
}

# ---- 规则 1：glimmer 源码 + 公共头的包含白名单 ----
$glimmerSources = Get-ChildItem -Recurse -Path (Join-Path $root 'src/glimmer') -Filter *.c -File
foreach ($f in $glimmerSources) {
  foreach ($inc in Get-LumIncludes $f.FullName) {
    if (-not (Test-Allowed $inc)) {
      $violations.Add("规则1 glimmer 源码越界包含：$($f.FullName.Substring($root.Length + 1)) -> $inc")
    }
  }
}
$glimmerHeaders = Get-ChildItem -Path (Join-Path $root 'include/glimmer') -Filter *.h -File
foreach ($h in $glimmerHeaders) {
  foreach ($inc in Get-LumIncludes $h.FullName) {
    if (-not (Test-Allowed $inc)) {
      $violations.Add("规则1 glimmer 公共头越界包含：include/glimmer/$($h.Name) -> $inc")
    }
  }
}

# ---- 规则 2：glimmer 公共头不得暴露 third_party ----
foreach ($h in $glimmerHeaders) {
  foreach ($inc in Get-LumIncludes $h.FullName) {
    if ($allowedThirdParty -contains $inc -or $inc -match '^(glad|GLFW|stb)') {
      $violations.Add("规则2 glimmer 公共头泄露第三方头：include/glimmer/$($h.Name) -> $inc")
    }
  }
}

# ---- 规则 3：所有 glimmer* 目标的链接行不得含 Luminous ----
# 覆盖 glimmer_hello / glimmer_hello3d / glimmer_check（凡是只该链接平台自己的目标）。
# 新增一个 glimmer* 示例时不用改这里 —— 它自动进入检查范围。
$linkChecked = 0
$linkSkipped = 0
$linkDirs = Get-ChildItem -Path (Join-Path $root 'build/CMakeFiles') -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like 'glimmer*' }
foreach ($dir in $linkDirs) {
  $linkTxt = Join-Path $dir.FullName 'link.txt'
  if (-not (Test-Path $linkTxt)) { ++$linkSkipped; continue }
  ++$linkChecked
  if ((Get-Content -Raw $linkTxt) -match 'Luminous') {
    $name = $dir.Name -replace '\.dir$', ''
    $violations.Add("规则3 $name 的链接行里出现了 Luminous —— glimmer 已不再独立")
  }
}
$linkNote = "（检查 $linkChecked 个目标，跳过 $linkSkipped 个未构建）"

Write-Host "glimmer 边界检查：$($glimmerSources.Count) 个源文件 / $($glimmerHeaders.Count) 个公共头；独立运行目标链接行 $linkNote"
if ($violations.Count -eq 0) {
  Write-Host "  通过：依赖方向为 glimmer -> Luminous，且 glimmer 可独立运行"
  exit 0
}
foreach ($v in $violations) { Write-Host "  ★ $v" }
exit 1
