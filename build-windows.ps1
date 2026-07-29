[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$buildDirectory = Join-Path $projectRoot "build\windows-x64"
$vsWhere = Join-Path ([Environment]::GetFolderPath("ProgramFilesX86")) "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "Visual Studio Installer (vswhere.exe) was not found. Install Visual Studio with the 'Desktop development with C++' workload."
}

$visualStudioPath = & $vsWhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $visualStudioPath) {
    throw "A Visual Studio installation with the C++ toolchain was not found. Add the 'Desktop development with C++' workload."
}

$visualStudioCMake = Join-Path $visualStudioPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path -LiteralPath $visualStudioCMake)) {
    throw "Visual Studio's bundled CMake was not found at: $visualStudioCMake"
}

$cmakeHelp = & $visualStudioCMake --help
$generator = @(
    "Visual Studio 18 2026"
    "Visual Studio 17 2022"
    "Visual Studio 16 2019"
) | Where-Object { $cmakeHelp -match [regex]::Escape($_) } | Select-Object -First 1

if (-not $generator) {
    throw "The bundled CMake does not expose a supported Visual Studio generator."
}

Write-Host "Visual Studio: $visualStudioPath"
Write-Host "CMake:        $visualStudioCMake"
Write-Host "Generator:    $generator"
Write-Host "Build:        $buildDirectory"

& $visualStudioCMake `
    -S $projectRoot `
    -B $buildDirectory `
    -G $generator `
    -A x64 `
    -DTERRAIN_FETCH_GLFW=OFF

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $visualStudioCMake `
    --build $buildDirectory `
    --config $Configuration `
    --parallel

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$executable = Join-Path $buildDirectory "bin\$Configuration\TerrainGeneration.exe"
Write-Host ""
Write-Host "Build completed successfully."
Write-Host "Run: $executable"
