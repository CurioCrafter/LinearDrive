param(
    [string]$ZipPath = "dist\LinearDrive-Windows.zip",
    [string]$OutputPath = "dist\LinearDriveSetup.exe",
    [string]$GxxPath = "C:\msys64\mingw64\bin\g++.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "tools\installer\LinearDriveInstaller.cpp"
$stubDir = Join-Path $root "build\installer"
$stub = Join-Path $stubDir "LinearDriveInstallerStub.exe"
$zip = Join-Path $root $ZipPath
$output = Join-Path $root $OutputPath

if (-not (Test-Path -LiteralPath $source)) {
    throw "Missing installer source: $source"
}
if (-not (Test-Path -LiteralPath $zip)) {
    throw "Missing package zip: $zip"
}
if (-not (Test-Path -LiteralPath $GxxPath)) {
    throw "Missing compiler: $GxxPath"
}

New-Item -ItemType Directory -Path $stubDir -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
$env:PATH = (Split-Path -Parent $GxxPath) + ";" + $env:PATH

& $GxxPath -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -mwindows -municode `
    $source -o $stub -lole32 -lshell32 -luuid
if ($LASTEXITCODE -ne 0) {
    throw "Installer stub compile failed with exit code $LASTEXITCODE"
}

if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output -Force
}

$magic = [System.Text.Encoding]::ASCII.GetBytes("LDRVPKG1")
$zipInfo = Get-Item -LiteralPath $zip
$outputStream = [System.IO.File]::Create($output)
try {
    foreach ($path in @($stub, $zip)) {
        $inputStream = [System.IO.File]::OpenRead($path)
        try {
            $inputStream.CopyTo($outputStream)
        } finally {
            $inputStream.Dispose()
        }
    }
    $sizeBytes = [System.BitConverter]::GetBytes([UInt64]$zipInfo.Length)
    $outputStream.Write($sizeBytes, 0, $sizeBytes.Length)
    $outputStream.Write($magic, 0, $magic.Length)
} finally {
    $outputStream.Dispose()
}

Get-Item -LiteralPath $output
