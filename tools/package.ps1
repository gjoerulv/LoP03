# Crystal Dungeons — release packaging (M24, owner-approved layout).
# One command from a VS developer shell:
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1
# Configures + builds the msvc-release preset, stages the distribution
# folder, VALIDATES it (required files, licenses, no debug artifacts, no
# capture CLI), and zips it as dist\CrystalDungeons-<version>-win64.zip.
# Fails loudly on any validation problem.

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

# Version comes from the single source: project(VERSION) in CMakeLists.txt.
$projectLine = (Get-Content CMakeLists.txt | Select-String -Pattern 'VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1)
if ($null -eq $projectLine) { throw 'package: could not read project VERSION from CMakeLists.txt' }
$version = $projectLine.Matches[0].Groups[1].Value
Write-Output "package: version $version"

# 1. Configure + build the release preset.
cmake --preset msvc-release | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'package: configure failed' }
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
foreach ($required in @('CrystalDungeons.exe', 'README.txt', 'LICENSES.txt',
                        'data\classes.json', 'data\enemies.json', 'data\items.json',
                        'data\skills.json', 'data\bosses.json', 'data\dungeon_themes.json',
                        'data\composition.json',
                        'assets\manifest.json', 'assets\credits.md')) {
  if (-not (Test-Path (Join-Path $stage $required))) { $problems += "missing: $required" }
}
# Every manifest path must exist inside the package.
$manifest = Get-Content (Join-Path $stage 'assets\manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest.assets) {
  if ($entry.path -and -not (Test-Path (Join-Path $stage "assets\$($entry.path)"))) {
    $problems += "manifest path missing in package: $($entry.path)"
  }
}
# No debug artifacts; no capture CLI in the shipped exe.
foreach ($forbidden in @('*.pdb', '*.ilk', '*.exp', '*.lib')) {
  if (Get-ChildItem $stage -Recurse -Filter $forbidden -ErrorAction SilentlyContinue) {
    $problems += "debug artifact present: $forbidden"
  }
}
if (Select-String -Path (Join-Path $stage 'CrystalDungeons.exe') -Pattern 'scenes clean' -Quiet) {
  $problems += 'capture CLI code present in the shipped exe'
}
# Version metadata must match the project version.
$fileVersion = (Get-Item (Join-Path $stage 'CrystalDungeons.exe')).VersionInfo.ProductVersion
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
