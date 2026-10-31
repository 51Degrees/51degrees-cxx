# Shared by the scripts in this folder, which dot-source it. Describes the
# three products this repository builds and tests, and works out which of
# them a pull request touches.

$AllProducts = @('common', 'dd', 'ipi')

# Paths that belong to a single product. A change anywhere else, including
# common, the CMake files and CI, affects every product.
$ProductPaths = @{
    dd = @('src/dd/', 'test/dd/', 'examples/dd/', 'docs/dd/', 'tools/dd/',
        'VisualStudio/DeviceDetection/', 'device-detection-data')
    ipi = @('src/ipi/', 'test/ipi/', 'examples/ipi/', 'docs/ipi/',
        'VisualStudio/IpIntelligence/', 'ip-intelligence-data')
}

# Visual Studio solutions built when an options entry uses msbuild.
$SolutionDirs = @{
    common = 'VisualStudio/Common'
    dd = 'VisualStudio/DeviceDetection'
    ipi = 'VisualStudio/IpIntelligence'
}

# Returns the products the changed paths affect.
function Resolve-AffectedProducts([string[]]$Changed) {
    if (!$Changed) { return $AllProducts }
    $affected = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $Changed) {
        $owner = $null
        foreach ($product in $ProductPaths.Keys) {
            foreach ($prefix in $ProductPaths[$product]) {
                if ($prefix.EndsWith('/') ? $path.StartsWith($prefix) :
                    ($path -eq $prefix -or $path.StartsWith("$prefix/"))) {
                    $owner = $product
                }
            }
        }
        if (!$owner) { return $AllProducts }
        if (!$affected.Contains($owner)) { $affected.Add($owner) }
    }
    return $affected.ToArray()
}

# Returns the products a pull request changes, or every product when the
# checkout is not a pull request (a publish, or a run on main).
function Get-AffectedProducts([string]$RepoPath) {
    $head = git -C $RepoPath rev-parse HEAD
    $base = git -C $RepoPath merge-base origin/main HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or !$base -or $base -eq $head) {
        $global:LASTEXITCODE = 0
        return $AllProducts
    }
    return Resolve-AffectedProducts (git -C $RepoPath diff --name-only $base $head)
}

# Returns the products an options entry asks for that this change affects.
function Get-Products([string[]]$Products, [string]$RepoPath) {
    $requested = $Products ? $Products : $AllProducts
    $affected = Get-AffectedProducts $RepoPath
    $selected = @($requested | Where-Object { $affected -contains $_ })
    $skipped = @($requested | Where-Object { $affected -notcontains $_ })
    if ($skipped) {
        Write-Host "Not affected by this change, so skipped: $($skipped -join ', ')"
    }
    return $selected
}

# Runs ctest on the tests of one product, writing JUnit results.
function Invoke-ProductTests {
    param(
        [Parameter(Mandatory)][string]$BuildPath,
        [Parameter(Mandatory)][string]$Product,
        [Parameter(Mandatory)][string]$Configuration,
        [Parameter(Mandatory)][string]$JUnitPath,
        [string]$TestsRegex,
        [string]$ExcludeRegex
    )
    $arguments = @('-C', $Configuration, '-T', 'test', '--no-compress-output',
        '--output-on-failure', '--output-junit', $JUnitPath,
        '--label-regex', "^$Product`$")
    if ($TestsRegex) { $arguments += @('--tests-regex', $TestsRegex) }
    if ($ExcludeRegex) { $arguments += @('--exclude-regex', $ExcludeRegex) }
    Push-Location $BuildPath
    try {
        Write-Host "Testing $Product in '$BuildPath'"
        & ctest @arguments | Out-Host
        return $LASTEXITCODE
    } finally {
        Pop-Location
    }
}
