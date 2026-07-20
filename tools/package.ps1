# Crystal Dungeons — validated Windows x64 release packaging.
# Run from an x64 Visual Studio developer shell:
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -notin @('x64', 'amd64')) {
    throw "package: target architecture is '$env:VSCMD_ARG_TGT_ARCH'; open an x64 developer shell"
}

$projectLine = (Get-Content CMakeLists.txt |
    Select-String -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' |
    Select-Object -First 1)
if ($null -eq $projectLine) {
    throw 'package: could not read project VERSION from CMakeLists.txt'
}
$version = $projectLine.Matches[0].Groups[1].Value
Write-Output "package: version $version"

# 1. Configure + build the release preset.
cmake --preset msvc-release | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'package: configure failed' }

$cache = Get-Content build-msvc-rel\CMakeCache.txt -Raw
foreach ($requiredOff in @('CRYSTAL_ENABLE_DEBUG_OVERLAY:BOOL=OFF',
                            'CRYSTAL_ENABLE_CAPTURE:BOOL=OFF')) {
    if (-not $cache.Contains($requiredOff)) {
        throw "package: release cache does not contain '$requiredOff'"
    }
}

cmake --build --preset release
if ($LASTEXITCODE -ne 0) { throw 'package: build failed' }

# 2. Stage.
$stage = Join-Path $repo "dist\CrystalDungeons-$version-win64"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item build-msvc-rel\CrystalDungeons.exe $stage
Copy-Item -Recurse data (Join-Path $stage 'data')
Copy-Item -Recurse assets (Join-Path $stage 'assets')
Copy-Item packaging\README-player.txt (Join-Path $stage 'README.txt')
Copy-Item packaging\LICENSES.txt (Join-Path $stage 'LICENSES.txt')

# 3. Validate the staged layout.
$problems = @()
foreach ($required in @(
    'CrystalDungeons.exe', 'README.txt', 'LICENSES.txt',
    'data\classes.json', 'data\enemies.json', 'data\items.json',
    'data\skills.json', 'data\bosses.json', 'data\dungeon_themes.json',
    'data\composition.json', 'assets\manifest.json', 'assets\credits.md')) {
    if (-not (Test-Path (Join-Path $stage $required))) {
        $problems += "missing: $required"
    }
}

$manifest = Get-Content (Join-Path $stage 'assets\manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest.assets) {
    if ($entry.path -and -not (Test-Path (Join-Path $stage "assets\$($entry.path)"))) {
        $problems += "manifest path missing in package: $($entry.path)"
    }
}

foreach ($forbidden in @('*.pdb', '*.ilk', '*.exp', '*.lib')) {
    if (Get-ChildItem $stage -Recurse -Filter $forbidden -ErrorAction SilentlyContinue) {
        $problems += "debug artifact present: $forbidden"
    }
}

$exe = Join-Path $stage 'CrystalDungeons.exe'
$exeBytes = [System.IO.File]::ReadAllBytes($exe)
if ($exeBytes.Length -lt 64) {
    $problems += 'executable is too small to be a valid PE image'
} else {
    $peOffset = [BitConverter]::ToInt32($exeBytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 6 -gt $exeBytes.Length) {
        $problems += 'invalid PE header offset'
    } else {
        $signature = [BitConverter]::ToUInt32($exeBytes, $peOffset)
        $machine = [BitConverter]::ToUInt16($exeBytes, $peOffset + 4)
        if ($signature -ne 0x00004550) {
            $problems += 'invalid PE signature'
        }
        if ($machine -ne 0x8664) {
            $problems += ('executable machine is 0x{0:X4}; expected AMD64 (0x8664)' -f $machine)
        }
    }
}

# Static strings provide a defense-in-depth check that dev-only surfaces were
# not linked into the shipping executable. CMake also has a compile-time guard.
$exeText = [Text.Encoding]::ASCII.GetString($exeBytes)
foreach ($marker in @('--capture', 'scenes clean', 'F1: toggle overlay', 'Reloading assets...')) {
    if ($exeText.Contains($marker)) {
        $problems += "forbidden debug/capture marker present in executable: $marker"
    }
}

$fileVersion = (Get-Item $exe).VersionInfo.ProductVersion
if ($fileVersion -notlike "$version*") {
    $problems += "exe ProductVersion '$fileVersion' does not match project version $version"
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "package: FAIL $_" }
    throw "package: validation failed with $($problems.Count) problem(s)"
}

# 4. Zip.
$zip = Join-Path $repo "dist\CrystalDungeons-$version-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Output "package: OK -> $zip ($size MB)"
