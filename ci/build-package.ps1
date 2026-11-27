param(
    [Parameter(Mandatory)][string]$RepoName
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

# A warning fails the build of the package, as the default of the CMake
# option of the same name.
$env:_51DEGREES_WARNINGS_AS_ERRORS = 'true'

# The package is the build folder, which install-package.ps1 copies back into
# the repository for the tests.
$BuildPath = New-Item -ItemType directory -Path $RepoName/build -Force
$OutPath = New-Item -ItemType directory -Path package -Force

Write-Output "Entering '$BuildPath'"
Push-Location $BuildPath
try {
    Write-Output "Building"

    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release

} finally {
    Write-Output "Leaving '$BuildPath'"
    Pop-Location
}

Write-Output "Copying build to '$OutPath'"
Copy-Item -Recurse -Path $BuildPath -Destination $OutPath
