[CmdletBinding()]
param(
    [string]$Distribution = "Ubuntu"
)

$ErrorActionPreference = "Stop"

function Get-WslDistributions {
    $output = (& wsl.exe --list --quiet) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not list WSL distributions. Run 'wsl --status' for details."
    }
    $cleanOutput = $output -replace [char]0, ""
    return @($cleanOutput.Split("`n", [System.StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { $_.Trim() })
}

$distributions = Get-WslDistributions
if ($Distribution -notin $distributions) {
    Write-Host "Installing the $Distribution WSL distribution..."
    & wsl.exe --install --distribution $Distribution --no-launch
    if ($LASTEXITCODE -ne 0) {
        throw "WSL could not install $Distribution. A Windows restart may be required."
    }
}

Write-Host "Installing the native test toolchain in $Distribution..."
& wsl.exe --distribution $Distribution --user root -- sh -lc "apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y clang cmake ninja-build zlib1g-dev"
if ($LASTEXITCODE -ne 0) {
    throw "The $Distribution WSL environment could not start or install packages. Run 'wsl --update --web-download', restart Windows if requested, then rerun this script."
}

& wsl.exe --set-default $Distribution
if ($LASTEXITCODE -ne 0) {
    throw "The toolchain was installed, but $Distribution could not be made the default WSL distribution."
}

Write-Host "Native test toolchain ready. Run .\vendor\ultima4-ios\tests\run-mobile-tests.ps1"
