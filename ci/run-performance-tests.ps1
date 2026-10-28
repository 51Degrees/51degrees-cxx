param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake",
    [string[]]$Products
)
$ErrorActionPreference = "Stop"
. $PSScriptRoot/products.ps1

if ($BuildMethod -ne "cmake") {
    Write-Output "No performance tests for '$BuildMethod' builds"
    exit 0
}

$Products = Get-Products -Products $Products -RepoPath $RepoName
$RepoPath = (Resolve-Path $RepoName).Path
$BuildPath = "$RepoPath/build"
$BinPath = $IsWindows ? "$BuildPath/bin/$Configuration" : "$BuildPath/bin"
$Exe = $IsWindows ? '.exe' : ''
$SummaryPath = New-Item -ItemType Directory -Force "$RepoPath/test-results/performance-summary"
$failed = 0

# The performance tests of each product. HighPerformance is the name of a
# configuration, not an indication that a test is a performance test.
$TestsRegex = @{ common = '.*Perf.*'; dd = '.*Performance.*'; ipi = '.*Performance.*' }
$ExcludeRegex = @{ common = ''; dd = '.*HighPerformance.*'; ipi = '.*HighPerformance.*' }

# Every product's figures go into one summary for this options entry. The
# metric names already differ between products.
$higher = [ordered]@{}

foreach ($product in $Products) {
    $code = Invoke-ProductTests -BuildPath $BuildPath -Product $product `
        -Configuration $Configuration -TestsRegex $TestsRegex[$product] `
        -ExcludeRegex $ExcludeRegex[$product] `
        -JUnitPath "$RepoPath/test-results/performance/$Name-$product.xml"
    # IP Intelligence performance tests warn rather than fail.
    if ($code -ne 0 -and $product -eq 'ipi') {
        Write-Warning "Performance tests for '$product' exited with code $code"
    } elseif ($code -ne 0) {
        Write-Output "Performance tests for '$product' failed with exit code $code"
        $failed = $code
    }

    # The examples find their evidence files by searching upward from the
    # working directory, so they run from the repository root.
    $output = "$RepoPath/summary-$product.json"
    Remove-Item $output -ErrorAction SilentlyContinue
    Push-Location $RepoPath
    try {
        switch ($product) {
            'common' { & "$BinPath/CachePerf$Exe" $output }
            'dd' {
                & "$BinPath/PerformanceHashC$Exe" --json-output $output `
                    --data-file "$RepoPath/device-detection-data/TAC-HashV41.hash"
            }
            'ipi' {
                & "$BinPath/PerformanceC$Exe" --json-output $output `
                    --data-file "$RepoPath/ip-intelligence-data/51Degrees-LiteV41.ipi" `
                    --ip-addresses-file "$RepoPath/ip-intelligence-data/evidence.yml"
            }
        }
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($code -ne 0 -or !(Test-Path $output)) {
        if ($product -eq 'ipi') {
            Write-Warning "Performance run for '$product' exited with code $code"
        } else {
            Write-Output "Performance run for '$product' failed with exit code $code"
            $failed = $code ? $code : 1
        }
        continue
    }

    $results = Get-Content $output | ConvertFrom-Json -AsHashtable
    switch ($product) {
        'common' {
            $higher.CacheFetchesPerSecond = $results.CacheFetchesPerSecond
            $higher.CacheFetchesPerSecondPerThread = $results.CacheFetchesPerSecondPerThread
        }
        'dd' { $higher.DetectionsPerSecond = $results.InMemory.DetectionsPerSecond }
        'ipi' { $higher.LookupsPerSecond = $results.InMemory.LookupsPerSecond }
    }
}

@{ HigherIsBetter = $higher; LowerIsBetter = @{} } | ConvertTo-Json -Depth 3 |
    Set-Content "$SummaryPath/results_$Name.json"
Get-Content "$SummaryPath/results_$Name.json"

exit $failed
