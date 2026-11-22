<#
.SYNOPSIS
Runs the test programs of one Visual Studio build.

.DESCRIPTION
Called by ci/run-unit-tests.ps1, from the folder that holds the clone of the
repository. Runs every program in the build folder whose name contains Test,
each of which is a GoogleTest program, and writes its results to
test-results/unit in the repository.

.PARAMETER ProjectDir
The folder of the solution, relative to the repository.

.PARAMETER BuildDir
The folder the build wrote to, relative to the solution folder.
#>
param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$ProjectDir = ".",
    [string]$Name,
    [string]$Configuration = "Release",
    [string]$Arch = "x64",
    [string]$BuildMethod = "msbuild",
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

if ($BuildMethod -ne "msbuild") {
    Write-Error "The build method '$BuildMethod' is not supported."
}

$RepoPath = "$PWD/$RepoName"
$BuildPath = "$RepoPath/$ProjectDir/$BuildDir"

Write-Output "Entering '$BuildPath'"
Push-Location $BuildPath
try {
    $TestBinaries = Get-ChildItem -Filter *Test*.exe

    foreach ($TestBinary in $TestBinaries) {
        Write-Output $TestBinary.FullName
        Write-Output "Testing $Name-$($TestBinary.Name)"
        $XmlOutPath = "$RepoPath\test-results\unit\${Name}_$($TestBinary.BaseName).xml"
        & $TestBinary.FullName --gtest_catch_exceptions=1 --gtest_break_on_failure=0 --gtest_output=xml:$XmlOutPath
        Write-Output "Results saved to $XmlOutPath"
    }

} finally {
    Write-Output "Leaving '$BuildPath'"
    Pop-Location
}
