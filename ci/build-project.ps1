param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake",
    [string[]]$Products,
    [bool]$IpiSymbols = $true,
    [bool]$CxxBothProducts
)
$ErrorActionPreference = "Stop"
. $PSScriptRoot/products.ps1

$Products = Get-Products -Products $Products -RepoPath $RepoName
if (!$Products) {
    Write-Output "Nothing in '$Name' is affected by this change"
    exit 0
}

function OnOff([bool]$value) { $value ? 'ON' : 'OFF' }

if ($BuildMethod -eq "cmake") {

    # One configure builds every product in the entry. The large data file
    # copy of the common tests runs on x64 only.
    ./cxx/build-project.ps1 -RepoName $RepoName -ProjectDir . -Name $Name `
        -Configuration $Configuration -Arch $Arch -BuildMethod $BuildMethod `
        -ExtraArgs @(
            "-DFIFTYONE_BUILD_DEVICE_DETECTION=$(OnOff ($Products -contains 'dd'))"
            "-DFIFTYONE_BUILD_IP_INTELLIGENCE=$(OnOff ($Products -contains 'ipi'))"
            "-DFIFTYONE_COMMON_CXX_BUILD_TESTING=$(OnOff ($Products -contains 'common'))"
            "-DFIFTYONE_COMMON_CXX_BUILD_TESTING_IPI=$(OnOff ($Arch -eq 'x64'))"
            "-DFIFTYONE_IPI_SYMBOLS=$(OnOff $IpiSymbols)"
            "-DFIFTYONE_CXX_BOTH_PRODUCTS=$(OnOff $CxxBothProducts)"
        )

    # CMake builds both copies of common in these cases.
    if ($IpiSymbols -and ($Products -contains 'ipi' -or
        ($Products -contains 'common' -and $Arch -eq 'x64'))) {
        & $PSScriptRoot/check-common-symbols.ps1 -BuildPath "$RepoName/build" `
            -Cxx:$CxxBothProducts
    }

} elseif ($BuildMethod -eq "msbuild") {

    foreach ($product in $Products) {
        if (!$SolutionDirs.ContainsKey($product)) {
            Write-Output "No Visual Studio build for '$product'"
            continue
        }
        ./cxx/build-project.ps1 -RepoName $RepoName -ProjectDir $SolutionDirs[$product] `
            -Name "$Name-$product" -Configuration $Configuration -Arch $Arch `
            -BuildMethod $BuildMethod
    }

    # Common is also built with large data file support, x64 release only.
    if ($Products -contains 'common' -and $Arch -eq "x64" -and
        !$Configuration.StartsWith("Debug")) {
        ./cxx/build-project.ps1 -RepoName $RepoName -ProjectDir $SolutionDirs.common `
            -Name "${Name}_LargeDataFiles" -BuildDir "build-LargeDataFiles" `
            -Configuration "$Configuration-LargeDataFiles" -Arch $Arch `
            -BuildMethod $BuildMethod
    }

} else {
    Write-Error "The build method '$BuildMethod' is not supported."
}

exit $LASTEXITCODE
