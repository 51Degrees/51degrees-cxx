param (
    # The products to fetch for. Every product when not given, which is how
    # the asset cache is filled.
    [string[]]$Products,
    # A build only entry runs nothing, so it needs no data files.
    [bool]$BuildOnly,
    # The standard set of a pull request runs no IP Intelligence test, so it
    # needs no IP Intelligence data file.
    [string]$Depth
)
$ErrorActionPreference = "Stop"
. $PSScriptRoot/products.ps1

if ($BuildOnly) {
    Write-Output "No data files are needed to build without tests"
    exit 0
}

$Products = $Products ? $Products : $AllProducts
$data = "$PSScriptRoot/../data"

# Every data file the tests read is an asset, kept in ./assets, which the run
# caches once, so a test job restores the files rather than downloading them.
# The licensed files are downloaded with the license key in
# _51DEGREES_LICENSE_KEY.

# Points an example's data file variable at a file this script has put in
# place, so that no example downloads a data file in CI. It's set in this
# process for the scripts that run after this one in the same step, and in
# GITHUB_ENV for the steps after it.
function Set-DataFilePath([string]$Name, [string]$Path) {
    $fullPath = (Resolve-Path $Path).Path
    Set-Item "env:$Name" $fullPath
    if ($env:GITHUB_ENV) {
        Add-Content $env:GITHUB_ENV "$Name=$fullPath"
    }
    Write-Output "$Name is $fullPath"
}

if ($Products -contains 'dd') {
    $deviceDetectionData = "$data/device-detection"

    ./steps/fetch-assets.ps1 -Assets "TAC-HashV41.hash", "51Degrees-LiteV4.1.hash", "20000 Evidence Records.yml"

    Copy-Item "assets/TAC-HashV41.hash" $deviceDetectionData
    Copy-Item "assets/51Degrees-LiteV4.1.hash" $deviceDetectionData
    Copy-Item "assets/20000 Evidence Records.yml" $deviceDetectionData

    # The examples name the Lite file, so they get the same file as before.
    Set-DataFilePath '_51DEGREES_DD_PATH' "$deviceDetectionData/51Degrees-LiteV4.1.hash"
}

if ($Products -contains 'ipi' -and $Depth -ne 'standard') {
    $ipIntelligenceData = "$data/ip-intelligence"

    ./steps/fetch-assets.ps1 -Assets '51Degrees-EnterpriseIpiV41.ipi', '51Degrees-IPIV4AsnIpiV41.ipi'
    # The enterprise file stands in for the lite one too.
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-LiteV41.ipi"
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-EnterpriseIpiV41.ipi"
    Copy-Item "assets/51Degrees-IPIV4AsnIpiV41.ipi" $ipIntelligenceData

    # The Lite name already links to this file, so the examples get the same
    # file as before.
    Set-DataFilePath '_51DEGREES_IPI_PATH' "$ipIntelligenceData/51Degrees-EnterpriseIpiV41.ipi"

    Write-Host "Assets: $(@(Get-ChildItem assets).Count) files"

    Push-Location $ipIntelligenceData
    try {
        ./evidence-gen.ps1 -v4 10000 -v6 10000
    } finally {
        Pop-Location
    }
}
