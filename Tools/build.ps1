param(
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'Release',
    [ValidateSet('x64','Win32')][string]$Architecture = 'x64',
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Test,
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakeExe = $cmakeCommand.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install CMake 3.24+ or the Visual Studio C++ CMake tools.' }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmakeExe = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    if (-not (Test-Path -LiteralPath $cmakeExe)) { throw 'Install the Visual Studio C++ CMake tools, or add CMake to PATH.' }
}
$buildDirectory = Join-Path $projectRoot "build/vs-$Architecture"
if ($Clean) {
    if (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt')) {
        & $cmakeExe --build $buildDirectory --config $Configuration --target clean
        if ($LASTEXITCODE) { exit $LASTEXITCODE }
    }
    exit 0
}
& $cmakeExe -S $projectRoot -B $buildDirectory -A $Architecture
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$buildArguments = @('--build', $buildDirectory, '--config', $Configuration, '--parallel')
if ($Rebuild) { $buildArguments += '--clean-first' }
& $cmakeExe @buildArguments
if ($LASTEXITCODE) { exit $LASTEXITCODE }
if ($Test) {
    $ctestExe = Join-Path (Split-Path $cmakeExe) 'ctest.exe'
    & $ctestExe --test-dir $buildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
if ($Package) {
    & $cmakeExe --build $buildDirectory --config $Configuration --target package
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
Write-Host "Game: $buildDirectory/bin/$Configuration/SimpleGame.exe"
