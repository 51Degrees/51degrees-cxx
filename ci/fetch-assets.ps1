param (
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl,
    [string]$IpIntelligenceUrl,
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

# Downloads every file the data repository links to in one of its folders.
# A file that is already there is kept, so this runs after the assets are
# copied in and only fetches what they do not cover.
function Get-DataFiles([string]$Folder) {
    $global:LASTEXITCODE = 0
    & "$data/get-data-files.ps1" $Folder
    if ($LASTEXITCODE -ne 0) {
        throw "Not every data file in $Folder was downloaded"
    }
}

if ($Products -contains 'dd') {
    $deviceDetectionData = "$data/device-detection"

    ./steps/fetch-assets.ps1 -DeviceDetection:$DeviceDetection -DeviceDetectionUrl:$DeviceDetectionUrl `
        -Assets "TAC-HashV41.hash", "51Degrees-LiteV4.1.hash", "20000 Evidence Records.yml", "20000 User Agents.csv"

    Copy-Item "assets/TAC-HashV41.hash" $deviceDetectionData
    Copy-Item "assets/51Degrees-LiteV4.1.hash" $deviceDetectionData
    Copy-Item "assets/20000 Evidence Records.yml" $deviceDetectionData
    Copy-Item "assets/20000 User Agents.csv" $deviceDetectionData
    Get-DataFiles device-detection
}

if ($Products -contains 'ipi' -and $Depth -ne 'standard') {
    $ipIntelligenceData = "$data/ip-intelligence"

    ./steps/fetch-assets.ps1 -IpIntelligenceUrl:$IpIntelligenceUrl -Assets '51Degrees-EnterpriseIpiV41.ipi'
    # The enterprise file stands in for the lite one too.
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-LiteV41.ipi"
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-EnterpriseIpiV41.ipi"
    # Downloads the ASN file, which no asset covers.
    Get-DataFiles ip-intelligence

    Write-Host "Assets hashes:"
    Get-FileHash -Algorithm MD5 -Path assets/*

    Push-Location $ipIntelligenceData
    try {
        ./evidence-gen.ps1 -v4 10000 -v6 10000
        ./evidence-gen.ps1 -v4 10000 -v6 10000 -csv -path "evidence.csv"
    } finally {
        Pop-Location
    }
}
