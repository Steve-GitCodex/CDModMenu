[CmdletBinding()]
param(
    [string]$GameDir = 'C:\voices38\Crimson Desert',
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [string]$ModName = 'CDModMenu',
    [switch]$SkipBuild,
    [switch]$CloseGame,
    [switch]$Launch,
    [switch]$Rollback,
    [switch]$Uninstall,
    [int]$KeepBackups = 5
)

$ErrorActionPreference = 'Stop'
$RepoDir = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $RepoDir 'build'
$Bin64 = Join-Path $GameDir 'bin64'
$BackupRoot = Join-Path $GameDir "_${ModName}_backups"
$DeployedAsi = Join-Path $Bin64 "$ModName.asi"
$LegacyNames = @('Trinity')

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $fallback = 'C:\Program Files\CMake\bin\cmake.exe'
    if (Test-Path $fallback) { return $fallback }
    throw 'CMake not found. Install it with: winget install --id Kitware.CMake -e'
}

function Assert-GameDir {
    if (-not (Test-Path (Join-Path $Bin64 'CrimsonDesert.exe'))) {
        throw "CrimsonDesert.exe not found in '$Bin64'. Pass -GameDir."
    }
}

function Stop-GameIfRunning {
    $running = Get-Process -Name CrimsonDesert -ErrorAction SilentlyContinue
    if (-not $running) { return }
    if (-not $CloseGame) {
        throw 'Crimson Desert is running and has the .asi locked. Close it, or rerun with -CloseGame.'
    }
    Write-Step 'Closing Crimson Desert'
    $running | Stop-Process -Force
    $running | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
}

function Invoke-Build {
    $cmake = Find-CMake
    if (-not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
        Write-Step 'Configuring'
        & $cmake -S $RepoDir -B $BuildDir -G 'Visual Studio 17 2022' -A x64 "-DMOD_NAME=$ModName"
        if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    }
    Write-Step "Building ($Config)"
    & $cmake --build $BuildDir --config $Config -- /m /nologo /v:minimal
    if ($LASTEXITCODE) { throw 'Build failed - nothing was deployed.' }
}

function Save-Backup {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $dest = Join-Path $BackupRoot $stamp
    $files = @(Get-ChildItem $Bin64 -File | Where-Object {
        $_.BaseName -in (@($ModName) + $LegacyNames) -and $_.Extension -in '.asi', '.ini'
    })
    if ($files.Count -eq 0) { return }
    New-Item -ItemType Directory -Force $dest | Out-Null
    $files | Copy-Item -Destination $dest
    Write-Step "Backed up $($files.Count) file(s) to $dest"
    Get-ChildItem $BackupRoot -Directory | Sort-Object Name -Descending |
        Select-Object -Skip $KeepBackups | Remove-Item -Recurse -Force
}

function Remove-LegacyMods {
    foreach ($name in $LegacyNames) {
        $asi = Join-Path $Bin64 "$name.asi"
        $ini = Join-Path $Bin64 "$name.ini"
        $newIni = Join-Path $Bin64 "$ModName.ini"
        if ((Test-Path $ini) -and -not (Test-Path $newIni)) {
            Copy-Item $ini $newIni
            Write-Step "Migrated settings $name.ini -> $ModName.ini"
        }
        if (Test-Path $asi) {
            Remove-Item $asi -Force
            Write-Step "Removed $name.asi so two menus don't load"
        }
    }
}

function Install-Mod {
    $built = Join-Path $BuildDir "$Config\$ModName.asi"
    if (-not (Test-Path $built)) { throw "No build output at '$built'. Run without -SkipBuild." }
    Save-Backup
    Remove-LegacyMods
    Copy-Item $built $DeployedAsi -Force
    $same = (Get-FileHash $built).Hash -eq (Get-FileHash $DeployedAsi).Hash
    if (-not $same) { throw 'Copied file does not match the build output.' }
    $version = (Get-Item $DeployedAsi).VersionInfo.FileVersion
    Write-Step "Deployed $ModName v$version ($Config) -> $DeployedAsi"
}

function Restore-LatestBackup {
    $latest = Get-ChildItem $BackupRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $latest) { throw "No backups in '$BackupRoot'." }
    Remove-Item $DeployedAsi -Force -ErrorAction SilentlyContinue
    Get-ChildItem $latest.FullName -File | Copy-Item -Destination $Bin64 -Force
    Write-Step "Restored backup $($latest.Name)"
}

function Uninstall-Mod {
    Save-Backup
    Remove-Item $DeployedAsi -Force -ErrorAction SilentlyContinue
    Write-Step "Removed $ModName.asi (settings .ini kept; backup saved)"
}

Assert-GameDir
Stop-GameIfRunning

if ($Rollback) { Restore-LatestBackup }
elseif ($Uninstall) { Uninstall-Mod }
else {
    if (-not $SkipBuild) { Invoke-Build }
    Install-Mod
}

if ($Launch) {
    Write-Step 'Launching Crimson Desert'
    Start-Process -FilePath (Join-Path $Bin64 'CrimsonDesert.exe') -WorkingDirectory $Bin64
}
