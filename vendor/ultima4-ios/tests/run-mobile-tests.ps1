[CmdletBinding()]
param(
    [string]$Distribution = "Ubuntu"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$wslRepositoryRoot = ((& wsl.exe --distribution $Distribution -- wslpath -a $repositoryRoot) -replace [char]0, "").Trim()
if ($LASTEXITCODE -ne 0 -or -not $wslRepositoryRoot) {
    throw "Could not enter the $Distribution native-test environment. Run .\vendor\ultima4-ios\tests\bootstrap-windows-native-tests.ps1 first."
}

& wsl.exe --distribution $Distribution --cd $wslRepositoryRoot -- sh vendor/ultima4-ios/tests/run-mobile-tests.sh
if ($LASTEXITCODE -ne 0) {
    throw "Native mobile regression tests failed."
}
