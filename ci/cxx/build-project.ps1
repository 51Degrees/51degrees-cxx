<#
.SYNOPSIS
Builds one project folder of the repository with CMake or MSBuild.

.DESCRIPTION
Called by ci/build-project.ps1, from the folder that holds the clone of the
repository. With cmake, configures and builds the project in its build
folder. With msbuild, restores the NuGet packages and builds the solution in
the project folder.

.PARAMETER ProjectDir
The folder of the project, relative to the repository.

.PARAMETER BuildDir
The folder the build writes to, relative to the project folder.

.PARAMETER ExtraArgs
More arguments for the CMake configure.
#>
param(
    [Parameter(Mandatory)][string]$ProjectDir,
    [string]$RepoName,
    [string]$Name,
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake",
    [string]$BuildDir = "build",
    [string[]]$ExtraArgs
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

if ($BuildMethod -eq "cmake") {
    $RepoPath = New-Item -ItemType directory -Path $RepoName/$ProjectDir/$BuildDir -Force

    if ($IsWindows -and $Arch -eq "x86") {
        $ExtraArgs += '-A', 'win32'
    }

    Write-Output "Entering '$RepoPath'"
    Push-Location $RepoPath
    try {
        Write-Output "Building '$Name'"

        # The quotes are needed for $Configuration to expand.
        cmake .. -DCMAKE_BUILD_TYPE="$Configuration" $ExtraArgs

        cmake --build . --config $Configuration --parallel ([Environment]::ProcessorCount)

    } finally {
        Write-Output "Leaving '$RepoPath'"
        Pop-Location
    }

} elseif ($BuildMethod -eq "msbuild") {
    $RepoPath = New-Item -ItemType directory -Path $RepoName/$ProjectDir -Force

    Write-Output "Entering '$RepoPath'"
    Push-Location $RepoPath
    try {
        & {
            $PSNativeCommandUseErrorActionPreference = $false
            nuget restore || Write-Warning "nuget restore failed"
        }
        msbuild /p:Configuration=$Configuration /p:Platform=$Arch /p:OutDir=$RepoPath\$BuildDir\

    } finally {
        Write-Output "Leaving '$RepoPath'"
        Pop-Location
    }

} else {
    Write-Error "The build method '$BuildMethod' is not supported."
}
