# Build NFS13Unlimiter and pack dist\NFS13Unlimiter <version>.7z.
#   .\dist.ps1 -Version 1.0.0
param(
    [Parameter(Mandatory)][string]$Version,
    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe"
)

$root = $PSScriptRoot
$build = Join-Path $root "build"
cmake -S $root -B $build -A Win32
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$stage = Join-Path $root "dist\NFS13Unlimiter"
Remove-Item $stage -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $build "Release\NFS13Unlimiter.asi") $stage
Copy-Item (Join-Path $root "README.txt") $stage

$archive = Join-Path $root "dist\NFS13Unlimiter $Version.7z"
Remove-Item $archive -ErrorAction SilentlyContinue
& $SevenZip a -t7z -mx=9 -m0=lzma2 -md=256m -mfb=64 -ms=16g -mmt=18 $archive "$stage\*" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "7-Zip failed." }
Write-Host "Packed $archive" -ForegroundColor Green
