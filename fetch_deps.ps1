# Download libmem into external\ (5.1.5, 32-bit static library for /MD builds).
#   .\fetch_deps.ps1
param(
    [string]$Dest = (Join-Path $PSScriptRoot "external")
)

$ProgressPreference = "SilentlyContinue" # Invoke-WebRequest is very slow with the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$libmem = Join-Path $Dest "libmem-i686"
if (Test-Path (Join-Path $libmem "lib\release\libmem.lib")) { return }
New-Item -ItemType Directory -Force $Dest | Out-Null
$name = "libmem-5.1.5-i686-windows-msvc-static-md"
$tgz = Join-Path $env:TEMP "$name.tar.gz"
Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/rdbo/libmem/releases/download/5.1.5/$name.tar.gz" -OutFile $tgz
$tmp = Join-Path $env:TEMP $name
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $tmp | Out-Null
tar.exe -xzf $tgz -C $tmp
if ($LASTEXITCODE -ne 0) { throw "could not extract $tgz" }
$root = Get-ChildItem $tmp -Directory | Select-Object -First 1
Move-Item $root.FullName $libmem
Remove-Item $tmp, $tgz -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "libmem -> $libmem"
