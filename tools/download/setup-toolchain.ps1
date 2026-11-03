<#
.SYNOPSIS
    Install the pinned portable Windows toolchain, then build the firmware.
.PARAMETER ToolsDir
    Optional ASCII installation path. The choice is remembered for this checkout.
.PARAMETER InstallOnly
    Install and check tools without building. Used internally by wt.ps1.
#>
[CmdletBinding()]
param(
    [string]$ToolsDir,
    [switch]$InstallOnly,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Mode = 'Debug',
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$configFile = Join-Path $Root '.wt-toolchain.json'
$packages = (Import-PowerShellDataFile (Join-Path $PSScriptRoot 'toolchains.psd1')).Packages

function Test-Package([string]$Directory, $Package) {
    foreach ($file in $Package.RequiredFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $Directory $file) -PathType Leaf)) { return $false }
    }
    foreach ($check in $Package.Checks) {
        $exe = Join-Path $Directory ('bin\' + $check.Exe)
        if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { return $false }
        try {
            $output = @(& $exe $check.Argument 2>&1)
            if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch $check.Pattern) { return $false }
        } catch { return $false }
    }
    return $true
}

function Invoke-CurlDownload([string[]]$CurlArguments) {
    # Windows PowerShell 5.1 can turn native stderr (including progress) into
    # terminating errors when the caller redirects logs. Use curl's exit code.
    $ErrorActionPreference = 'Continue'
    & curl.exe @CurlArguments
    return $LASTEXITCODE
}

function Get-Archive($Package, [string]$CacheDir) {
    $archive = Join-Path $CacheDir $Package.Archive
    if (Test-Path -LiteralPath $archive) {
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ieq $Package.Sha256) {
            return $archive
        }
        # Preserve the bad download for diagnosis; never execute or unpack it.
        Move-Item -LiteralPath $archive -Destination ($archive + '.bad-' + [guid]::NewGuid().ToString('N'))
    }
    $partial = $archive + '.part'
    # A previous run may have finished downloading just before it was stopped.
    if ((Test-Path -LiteralPath $partial) -and
        (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ieq $Package.Sha256) {
        Move-Item -LiteralPath $partial -Destination $archive -Force
        return $archive
    }
    Write-Host ("Downloading {0} ..." -f $Package.Name)
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        $curlArgs = @('--fail', '--location', '--retry', '3', '--retry-delay', '2',
                      '--connect-timeout', '30', '--max-time', '1800',
                      '--continue-at', '-', '--output', $partial)
        # curl reads proxy environment variables but ignores Windows Internet
        # Options. Honour the system proxy too, including per-URL PAC decisions.
        if (-not $env:HTTPS_PROXY -and -not $env:ALL_PROXY) {
            $proxy = [Net.WebRequest]::DefaultWebProxy
            $uri = [uri]$Package.Url
            if ($proxy -and -not $proxy.IsBypassed($uri)) {
                $curlArgs += @('--proxy', $proxy.GetProxy($uri).AbsoluteUri)
            }
        }
        $downloadCode = Invoke-CurlDownload ($curlArgs + @($Package.Url))
        if ($downloadCode -eq 33) {
            # Some servers do not support Range. Preserve the partial file,
            # then retry from byte zero instead of getting stuck on every run.
            if (Test-Path -LiteralPath $partial) {
                Move-Item -LiteralPath $partial -Destination ($partial + '.no-range-' + [guid]::NewGuid().ToString('N'))
            }
            $downloadCode = Invoke-CurlDownload ($curlArgs + @($Package.Url))
        }
        if ($downloadCode -ne 0) { throw "Download failed. Check the network/proxy, then rerun; partial file: $partial" }
    } else {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -UseBasicParsing -Uri $Package.Url -OutFile $partial -TimeoutSec 1800
    }
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ine $Package.Sha256) {
        Move-Item -LiteralPath $partial -Destination ($partial + '.bad-' + [guid]::NewGuid().ToString('N'))
        throw "SHA256 mismatch for $($Package.Name). Rerun to download a clean archive."
    }
    Move-Item -LiteralPath $partial -Destination $archive -Force
    return $archive
}

try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    if (-not $ToolsDir -and (Test-Path -LiteralPath $configFile)) {
        try {
            $saved = Get-Content -LiteralPath $configFile -Raw | ConvertFrom-Json
            if (Test-Path -LiteralPath $saved.ToolsDir -PathType Container) { $ToolsDir = $saved.ToolsDir }
        } catch { Write-Warning 'Ignoring an invalid or outdated .wt-toolchain.json.' }
    }
    if (-not $ToolsDir) {
        $ToolsDir = Join-Path $env:LOCALAPPDATA 'WheeltecTool'
        if ($ToolsDir -match '[^\x20-\x7E]') { $ToolsDir = Join-Path $env:PUBLIC 'WheeltecTool' }
    }
    $ToolsDir = [IO.Path]::GetFullPath($ToolsDir)
    if ($ToolsDir -match '[^\x20-\x7E]') { throw 'ToolsDir must be an ASCII path, e.g. C:\Users\Public\WheeltecTool.' }
    $cache = Join-Path $ToolsDir 'downloads'
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $binDirs = @()
    foreach ($package in $packages) {
        $directory = Join-Path $ToolsDir $package.Directory
        if (-not (Test-Package $directory $package)) {
            $archive = Get-Archive $package $cache
            $stage = Join-Path $ToolsDir ('unpack-' + [guid]::NewGuid().ToString('N'))
            New-Item -ItemType Directory -Path $stage | Out-Null
            Write-Host ("Extracting {0} ..." -f $package.Name)
            if (Get-Command tar.exe -ErrorAction SilentlyContinue) {
                & tar.exe -xf $archive -C $stage
                if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $archive" }
            } else {
                Expand-Archive -LiteralPath $archive -DestinationPath $stage
            }
            $unpacked = Join-Path $stage $package.Directory
            if (-not (Test-Package $unpacked $package)) { throw "Tool validation failed after extraction: $unpacked" }
            # Both paths are fixed children of the selected tool directory.
            foreach ($target in @($directory, $unpacked)) {
                if (-not ([IO.Path]::GetFullPath($target)).StartsWith($ToolsDir.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Unsafe installation path: $target"
                }
            }
            if (Test-Path -LiteralPath $directory) {
                Move-Item -LiteralPath $directory -Destination ($directory + '.old-' + [guid]::NewGuid().ToString('N'))
            }
            Move-Item -LiteralPath $unpacked -Destination $directory
            # Only the now-empty staging folder is removed, non-recursively.
            Remove-Item -LiteralPath $stage
        }
        if (-not $Quiet) { Write-Host ("OK: {0} ({1})" -f $package.Name, $directory) }
        $binDirs += Join-Path $directory 'bin'
    }
    # xPack ships native Windows GNU Make as make.exe; CMake's MinGW generator
    # looks for mingw32-make.exe. Keep the complete licensed distribution.
    $makeBin = $binDirs[-1]
    $makeSource = Join-Path $makeBin 'make.exe'
    $makeAlias = Join-Path $makeBin 'mingw32-make.exe'
    if (-not (Test-Path -LiteralPath $makeAlias) -or
        (Get-FileHash -LiteralPath $makeSource -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $makeAlias -Algorithm SHA256).Hash) {
        Copy-Item -LiteralPath $makeSource -Destination $makeAlias -Force
    }
    $env:Path = ($binDirs -join ';') + ';' + $env:Path
    # Process-local PATH: no machine/user PATH changes, no conflict with Conda.
    $json = @{ ToolsDir = $ToolsDir } | ConvertTo-Json
    [IO.File]::WriteAllText($configFile + '.tmp', $json, (New-Object Text.UTF8Encoding($false)))
    Move-Item -LiteralPath ($configFile + '.tmp') -Destination $configFile -Force
    if (-not $InstallOnly) {
        & (Join-Path $Root 'tools\build\wt.ps1') -Mode $Mode -Quiet:$Quiet
        exit $LASTEXITCODE
    }
    exit 0
} catch {
    Write-Host ('Toolchain setup failed: ' + $_.Exception.Message) -ForegroundColor Red
    exit 2
}
