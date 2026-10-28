param (
    [string]$IpIntelligenceUrl
)
$ErrorActionPreference = "Stop"

$ipIntelligenceData = "$PSScriptRoot/../ip-intelligence-data"

./steps/fetch-assets.ps1 -IpIntelligenceUrl:$IpIntelligenceUrl -Assets '51Degrees-EnterpriseIpiV41.ipi'
New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-LiteV41.ipi" # Use Enterprise as Lite
New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/51Degrees-EnterpriseIpiV41.ipi" -Path "$ipIntelligenceData/51Degrees-EnterpriseIpiV41.ipi"

Write-Host "Assets hashes:"
Get-FileHash -Algorithm MD5 -Path assets/*

Push-Location $ipIntelligenceData
try {
    ./evidence-gen.ps1 -v4 10000 -v6 10000
    ./evidence-gen.ps1 -v4 10000 -v6 10000 -csv -path "evidence.csv"
} finally {
    Pop-Location
}
