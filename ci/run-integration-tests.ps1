param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake",
    [string[]]$Products,
    [bool]$BuildOnly
)
$ErrorActionPreference = "Stop"
. $PSScriptRoot/products.ps1

if ($BuildOnly) {
    Write-Output "'$Name' is built without its tests"
    exit 0
}

if ($BuildMethod -ne "cmake") {
    # The Visual Studio builds have no CTest tests, so nothing runs here.
    Write-Output "No integration tests for '$BuildMethod' builds"
    exit 0
}

$Products = Get-Products -Products $Products -RepoPath $RepoName
$RepoPath = (Resolve-Path $RepoName).Path
$failed = 0

# Each product excludes its own long running tests.
$Excludes = @{
    common = @()
    dd = @('.*OfflineProcessing.*')
    ipi = @(
        '.*OfflineProcessing.*'
        # The performance suites run again in run-performance-tests.ps1,
        # which is where their results are collected from. Match them by
        # suite name rather than ".*Performance.*", which would also catch
        # the HighPerformance configuration variants of every example.
        'ExampleTestPerformance(Legacy)?\.'
    )
}
if ($IsWindows) {
    # These exercise the file-backed collections, which are an order of
    # magnitude slower on Windows than on Linux - ExampleTestMem.LowMemory
    # alone is 260s here against 4s on Ubuntu. The Ubuntu jobs still run
    # them, and ExampleTestReloadFromFile keeps its Default and Balanced cases.
    $Excludes.ipi += 'ExampleTestMem\.LowMemory'
    $Excludes.ipi += 'ExampleTestReloadFromFile\.(InMemory|LowMemory|HighPerformance)'
}

foreach ($product in $Products) {
    $code = Invoke-ProductTests -BuildPath "$RepoPath/build" -Product $product `
        -Configuration $Configuration -TestsRegex '.*Integration|Example.*' `
        -ExcludeRegex ($Excludes[$product] -join '|') `
        -JUnitPath "$RepoPath/test-results/integration/$Name-$product.xml"
    if ($code -ne 0) {
        Write-Output "Integration tests for '$product' failed with exit code $code"
        $failed = $code
    }
}

exit $failed
