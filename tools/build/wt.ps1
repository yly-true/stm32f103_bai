<#
.SYNOPSIS
    WHEELTEC 倒立摆固件命令行构建工具（arm-none-eabi-gcc + CMake + MinGW Make）。

.DESCRIPTION
    一条命令完成：自动配置便携工具链 -> 准备 ASCII 构建路径 -> 配置 CMake -> 编译 -> 生成 hex/bin ->
    打印固件大小与 FlyMcu 烧录提示。

    工程目录名含中文时，mingw32-make 会读乱路径，因此本工具会自动在 ASCII 位置
    建立一个目录联接（junction）再从该路径构建；产物仍然落在本目录的 build\ 下。

.PARAMETER Mode
    构建类型：Debug（默认，-Og -g）、Release（-Ofast）、RelWithDebInfo、MinSizeRel。

.PARAMETER Clean
    先删除 build\ 再完整重新配置和编译。

.PARAMETER Rebuild
    同 -Clean（保留以便习惯用法）。

.PARAMETER Jobs
    并行编译任务数，默认交给 CMake/CPU 决定。

.PARAMETER Flash
    构建后启动 tools/flymcu/FlyMcu.exe，并把 hex 路径复制到剪贴板。

.PARAMETER Open
    构建后用资源管理器打开 build\ 目录。

.PARAMETER Quiet
    不打印每一步的明细（只输出结果与错误）。

.EXAMPLE
    .\wt.ps1
    调试构建。

.EXAMPLE
    .\wt.cmd -Mode Release -Flash
    发布构建，产出后打开 FlyMcu 准备串口烧录。

.EXAMPLE
    .\wt.ps1 -Clean -Mode Release
    清空构建目录后重新做一次 Release 构建。
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Mode = 'Debug',

    [switch]$Clean,
    [switch]$Rebuild,
    [int]$Jobs = 0,
    [switch]$Flash,
    [switch]$Open,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ---------------------------------------------------------------- 基础路径 ---
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not (Test-Path -LiteralPath (Join-Path $Root 'CMakeLists.txt'))) {
    throw "wt.ps1 必须在包含 CMakeLists.txt 的工程目录中运行（当前：$Root）"
}

function Write-Step([string]$Text) {
    if (-not $Quiet) { Write-Host "==> $Text" -ForegroundColor Cyan }
}
function Write-Ok([string]$Text)   { Write-Host "    $Text" -ForegroundColor DarkGray }
function Write-Fail([string]$Text) { Write-Host "!!  $Text" -ForegroundColor Red }

function Test-AsciiPath([string]$Path) {
    return ($Path -match '^[\x20-\x7E]+$')
}

# ------------------------------------------------------------ 工具链解析 ---
# 在陌生电脑上自动安装固定版本；便携工具只加入当前进程 PATH。
& (Join-Path $Root 'tools\download\setup-toolchain.ps1') -InstallOnly -Quiet:$Quiet
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$required = @{
    'arm-none-eabi-gcc' = 'Arm GNU Toolchain（arm-none-eabi-gcc）'
    'cmake'             = 'CMake 3.29.6+'
    'mingw32-make'      = 'MinGW Make（mingw32-make）'
}
foreach ($name in $required.Keys) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Fail "工具链配置后仍找不到 $name：请重新运行 .\tools\download\setup-toolchain.cmd。"
        exit 2
    }
}

# ---------------------------------------------------------- ASCII 构建路径 ---
# CMake 自身能处理中文路径，但 mingw32-make 读取的 Makefile 路径会乱码，
# 因此需要从一个纯 ASCII 路径进入工程。
$SourcePath = $Root
if (-not (Test-AsciiPath $Root)) {
    $hash = [System.BitConverter]::ToString(
        [System.Security.Cryptography.SHA1]::Create().ComputeHash(
            [Text.Encoding]::UTF8.GetBytes($Root.ToLowerInvariant())))
    $hash = $hash.Replace('-', '').Substring(0, 8)
    $toolRoot = (Get-Content -LiteralPath (Join-Path $Root '.wt-toolchain.json') -Raw | ConvertFrom-Json).ToolsDir

    $candidates = @(
        (Join-Path (Split-Path -Parent $Root) "wt-src-$hash"),
        (Join-Path $toolRoot "source-links\wt-src-$hash"),
        (Join-Path $env:LOCALAPPDATA "WheeltecTool\wt-src-$hash"),
        (Join-Path $env:SystemDrive "WheeltecTool\wt-src-$hash")
    ) | Where-Object { $_ -and (Test-AsciiPath $_) }

    $link = $null
    foreach ($candidate in $candidates) {
        try {
            if (Test-Path -LiteralPath $candidate) {
                $item = Get-Item -LiteralPath $candidate -Force
                $isLink = [bool]($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)
                if (-not $isLink) { continue }   # 真实目录，不能用，换下一个位置
                [System.IO.Directory]::Delete($candidate)
            }
            $parent = Split-Path -Parent $candidate
            if (-not (Test-Path -LiteralPath $parent)) {
                New-Item -ItemType Directory -Path $parent -Force | Out-Null
            }
            New-Item -ItemType Junction -Path $candidate -Target $Root | Out-Null
            $link = $candidate
            break
        } catch {
            if (-not $Quiet) { Write-Ok "跳过联接位置 $candidate（$($_.Exception.Message)）" }
        }
    }

    if (-not $link) {
        Write-Fail '无法创建 ASCII 目录联接；请把工程移到纯英文路径后重试。'
        exit 3
    }
    $SourcePath = $link
    Write-Step "ASCII 构建路径：$link"
}

$BuildDir = Join-Path $SourcePath 'build'
$CacheFile = Join-Path $BuildDir 'CMakeCache.txt'

# --------------------------------------------------------------- 配置阶段 ---
function Get-CachedValue([string]$Key) {
    if (-not (Test-Path -LiteralPath $CacheFile)) { return $null }
    $line = Select-String -LiteralPath $CacheFile -Pattern "^$([regex]::Escape($Key)):[^=]*=(.*)$" |
            Select-Object -First 1
    if ($line) { return $line.Matches[0].Groups[1].Value }
    return $null
}

function Get-CachedCompiler {
    $cached = Get-CachedValue 'CMAKE_C_COMPILER'
    if ($cached) { return $cached }
    # This project's normal CMAKE_C_COMPILER variable is recorded in the
    # compiler metadata, not necessarily in CMakeCache.txt.
    $files = Join-Path $BuildDir 'CMakeFiles'
    if (Test-Path -LiteralPath $files) {
        foreach ($directory in (Get-ChildItem -LiteralPath $files -Directory)) {
            $info = Join-Path $directory.FullName 'CMakeCCompiler.cmake'
            if (Test-Path -LiteralPath $info) {
                $line = Select-String -LiteralPath $info -Pattern '^set\(CMAKE_C_COMPILER "([^"]+)"\)'
                if ($line) { return $line.Matches[0].Groups[1].Value }
            }
        }
    }
    return $null
}

function Test-SameTool([string]$Cached, [string]$Current) {
    if (-not $Cached -or -not (Test-Path -LiteralPath $Cached -PathType Leaf)) { return $false }
    # Expand 8.3 names. CMake may also record the physical MSIX-redirected path
    # when invoked from a packaged app; equal executable hashes handle that.
    if ((Get-Item -LiteralPath $Cached).FullName -ieq (Get-Item -LiteralPath $Current).FullName) { return $true }
    return (Get-FileHash -LiteralPath $Cached -Algorithm SHA256).Hash -eq
           (Get-FileHash -LiteralPath $Current -Algorithm SHA256).Hash
}

$needsConfigure = $true
$reason = '首次配置'
if (Test-Path -LiteralPath $CacheFile) {
    $cachedMode = Get-CachedValue 'CMAKE_BUILD_TYPE'
    $cachedHome = Get-CachedValue 'CMAKE_HOME_DIRECTORY'
    $cachedCompiler = Get-CachedCompiler
    $cachedMake = Get-CachedValue 'CMAKE_MAKE_PROGRAM'
    $cachedCmake = Get-CachedValue 'CMAKE_COMMAND'
    $compiler = (Get-Command arm-none-eabi-gcc).Source.Replace('/', '\')
    $make = (Get-Command mingw32-make).Source.Replace('/', '\')
    $cmake = (Get-Command cmake).Source.Replace('/', '\')
    if ($cachedHome) { $cachedHome = $cachedHome.Replace('/', '\') }
    if ($cachedHome -and ($cachedHome.TrimEnd('/\') -ine $SourcePath.TrimEnd('/\'))) {
        $reason = "构建目录属于其它源码路径（$cachedHome）"
        $needsConfigure = $true
    } elseif (-not (Test-SameTool $cachedCompiler $compiler) -or
              -not (Test-SameTool $cachedMake $make) -or
              -not (Test-SameTool $cachedCmake $cmake)) {
        $reason = '编译工具位置变化（例如工程搬到了新电脑）'
        $needsConfigure = $true
    } elseif ($cachedMode -and ($cachedMode -ne $Mode)) {
        $reason = "构建类型从 $cachedMode 切换为 $Mode"
        $needsConfigure = $true
    } else {
        $needsConfigure = $false
    }
}

# 删除前检查实际目标：build 必须是源码目录下的普通子目录。
if (Test-Path -LiteralPath $BuildDir) {
    $sourceResolved = (Resolve-Path -LiteralPath $SourcePath).Path.TrimEnd('\')
    $buildResolved = (Resolve-Path -LiteralPath $BuildDir).Path
    $buildItem = Get-Item -LiteralPath $BuildDir -Force
    if ($buildResolved -ine (Join-Path $sourceResolved 'build') -or
        ($buildItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "拒绝清理不属于本工程的构建目录：$buildResolved"
    }
}

if ($Clean -or $Rebuild) {
    if (Test-Path -LiteralPath $BuildDir) {
        Write-Step '清理 build 目录'
        [System.IO.Directory]::Delete($BuildDir, $true)
    }
    $needsConfigure = $true
    $reason = '按 -Clean 要求完整重建'
} elseif ($needsConfigure -and (Test-Path -LiteralPath $BuildDir)) {
    Write-Step "重新配置（$reason）"
    [System.IO.Directory]::Delete($BuildDir, $true)
}

if ($needsConfigure) {
    Write-Step "配置 CMake（$Mode）"
    $cmakeArgs = @('-S', $SourcePath, '-B', $BuildDir, '-G', 'MinGW Makefiles', "-DCMAKE_BUILD_TYPE=$Mode")
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { Write-Fail 'CMake 配置失败。'; exit 4 }
}

# --------------------------------------------------------------- 编译阶段 ---
Write-Step "编译（$Mode）"
$buildArgs = @('--build', $BuildDir)
if ($Jobs -gt 0) { $buildArgs += @('--parallel', $Jobs) } else { $buildArgs += '--parallel' }

$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmake @buildArgs
$code = $LASTEXITCODE
$sw.Stop()
if ($code -ne 0) { Write-Fail '编译失败。'; exit 5 }

# --------------------------------------------------------------- 结果汇总 ---
$elf = Join-Path $BuildDir 'WHEELTEC.elf'
$hex = Join-Path $BuildDir 'WHEELTEC.hex'
$bin = Join-Path $BuildDir 'WHEELTEC.bin'

# POST_BUILD does not run again when ELF is up to date but HEX/BIN was removed.
foreach ($output in @(@{ Path = $hex; Format = 'ihex' }, @{ Path = $bin; Format = 'binary' })) {
    if (-not (Test-Path -LiteralPath $output.Path) -or
        (Get-Item -LiteralPath $output.Path).LastWriteTimeUtc -lt (Get-Item -LiteralPath $elf).LastWriteTimeUtc) {
        & arm-none-eabi-objcopy ('-O' + $output.Format) $elf $output.Path
        if ($LASTEXITCODE -ne 0) { Write-Fail "生成固件失败：$($output.Path)"; exit 5 }
    }
}

Write-Host ''
Write-Host "构建成功（$Mode，$([math]::Round($sw.Elapsed.TotalSeconds, 1)) s）" -ForegroundColor Green
$report = & arm-none-eabi-size $elf
if ($LASTEXITCODE -eq 0) {
    $parts = @(($report[1] -split '\s+') | Where-Object { $_ })
    $textSize = [int]$parts[0]
    $dataSize = [int]$parts[1]
    $bssSize = [int]$parts[2]
    $flashSize = $textSize + $dataSize
    $ramSize = $dataSize + $bssSize
    $flashPct = ($flashSize / 65536 * 100).ToString('0.0')
    $ramPct = ($ramSize / 20480 * 100).ToString('0.0')
    $line = '    FLASH {0,6} B / 65536 B ({1,5}%)    RAM {2,5} B / 20480 B ({3,5}%)' -f
        $flashSize, $flashPct, $ramSize, $ramPct
    Write-Host $line
}
foreach ($file in @($hex, $bin)) {
    if (Test-Path -LiteralPath $file) {
        $fileSize = (Get-Item -LiteralPath $file).Length
        $shown = $file.Replace($SourcePath, $Root)
        Write-Host ('    {0,-40} {1,8:N0} B' -f $shown, $fileSize)
    }
}
Write-Host ('    ELF: {0}' -f $elf.Replace($SourcePath, $Root)) -ForegroundColor DarkGray

# --------------------------------------------------------- FlyMcu / 打开目录 ---
if ($Open) {
    Start-Process -FilePath 'explorer.exe' -ArgumentList "`"$BuildDir`""
}

if ($Flash) {
    $flymcu = Join-Path $Root 'tools\flymcu\FlyMcu.exe'
    if (Test-Path -LiteralPath $flymcu) {
        try { Set-Clipboard -Value $hex.Replace($SourcePath, $Root) } catch { }
        Write-Step '启动 FlyMcu'
        Start-Process -FilePath $flymcu -WorkingDirectory (Split-Path $flymcu)
        Write-Host '    hex 路径已复制到剪贴板，在 FlyMcu 中粘贴到“程序文件”即可。' -ForegroundColor DarkGray
        Write-Host '    烧录前：BOOT0=1、BOOT1=0，按复位进入串口 ISP；烧录后 BOOT0=0 再复位运行。' -ForegroundColor DarkGray
    } else {
        Write-Fail "未找到 FlyMcu.exe：$flymcu"
    }
} else {
    Write-Host "`n烧录用 FlyMcu：打开 build\\WHEELTEC.hex（BOOT0=1、BOOT1=0 进入 ISP，烧完 BOOT0=0 复位）。" -ForegroundColor DarkGray
}

exit 0
