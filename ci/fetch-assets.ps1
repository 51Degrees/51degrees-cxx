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

# Every data file the tests read is an asset, kept in ./assets, which the run
# caches once, so a test job restores the files rather than downloading them.

if ($Products -contains 'dd') {
    $deviceDetectionData = "$data/device-detection"

    ./steps/fetch-assets.ps1 -DeviceDetection:$DeviceDetection -DeviceDetectionUrl:$DeviceDetectionUrl `
        -Assets "TAC-HashV41.hash", "51Degrees-LiteV4.1.hash", "20000 Evidence Records.yml", "20000 User Agents.csv"

    Copy-Item "assets/TAC-HashV41.hash" $deviceDetectionData
    Copy-Item "assets/51Degrees-LiteV4.1.hash" $deviceDetectionData
    Copy-Item "assets/20000 Evidence Records.yml" $deviceDetectionData
    Copy-Item "assets/20000 User Agents.csv" $deviceDetectionData
}

if ($Products -contains 'ipi' -and $Depth -ne 'standard') {
    $ipIntelligenceData = "$data/ip-intelligence"

    ./steps/fetch-assets.ps1 -IpIntelligenceUrl:$IpIntelligenceUrl -Assets '51Degrees-EnterpriseIpiV41.ipi', '51Degrees-IPIV4AsnIpiV41.ipi'
    # The enterprise file stands in for the lite one too.
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-LiteV41.ipi"
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-EnterpriseIpiV41.ipi"
    Copy-Item "assets/51Degrees-IPIV4AsnIpiV41.ipi" $ipIntelligenceData

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
