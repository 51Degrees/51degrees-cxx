param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake",
    [string[]]$Products,
    [bool]$SkipLiteTests,
    [bool]$BuildOnly
)
$ErrorActionPreference = "Stop"
. $PSScriptRoot/products.ps1

if ($BuildOnly) {
    Write-Output "'$Name' is built without its tests"
    exit 0
}

$Products = Get-Products -Products $Products -RepoPath $RepoName
$RepoPath = (Resolve-Path $RepoName).Path
$EnterpriseHash = "$RepoPath/device-detection-data/TAC-HashV41.hash"
$failed = 0

# Device Detection's tests run with the enterprise data file, then again with
# only the lite file by hiding the enterprise one.
function Invoke-WithLiteOnly([scriptblock]$Test) {
    if ($SkipLiteTests) {
        Write-Output "Skipping tests with lite data file (SkipLiteTests is set)"
    } elseif (Test-Path $EnterpriseHash) {
        Move-Item $EnterpriseHash "$EnterpriseHash.bak" -Force
        try {
            & $Test
        } finally {
            Move-Item "$EnterpriseHash.bak" $EnterpriseHash -Force
        }
    } else {
        Write-Output "Enterprise data file not found, only ran tests with available data file"
    }
}

if ($BuildMethod -eq "msbuild") {

    foreach ($product in $Products) {
        if (!$SolutionDirs.ContainsKey($product)) { continue }
        # The IP Intelligence solution is built but not tested. Its googletest
        # package has no GTEST_SKIP, and the stand-in in its test project does
        # not stop a test skipped in SetUp, so the temp file tests crash when
        # CI is true. The cmake entries run these tests.
        if ($product -eq 'ipi') {
            Write-Output "Not running the tests of the Visual Studio '$product' build"
            continue
        }
        $run = {
            param($suffix)
            ./cxx/run-unit-tests.ps1 -RepoName $RepoName -ProjectDir $SolutionDirs[$product] `
                -Name "$Name-$product$suffix" -Configuration $Configuration -Arch $Arch `
                -BuildMethod $BuildMethod
            if ($LASTEXITCODE -ne 0) { $script:failed = $LASTEXITCODE }
        }
        & $run ''
        if ($product -eq 'dd') {
            Invoke-WithLiteOnly { & $run '-Lite' }
        }
    }
    if ($Products -contains 'common' -and $Arch -eq "x64" -and
        !$Configuration.StartsWith("Debug")) {
        ./cxx/run-unit-tests.ps1 -RepoName $RepoName -ProjectDir $SolutionDirs.common `
            -Name "${Name}_LargeDataFiles" -BuildDir "build-LargeDataFiles" `
            -Configuration "$Configuration-LargeDataFiles" -Arch $Arch `
            -BuildMethod $BuildMethod
        if ($LASTEXITCODE -ne 0) { $failed = $LASTEXITCODE }
    }
    exit $failed
}

if ($BuildMethod -ne "cmake") {
    Write-Error "The build method '$BuildMethod' is not supported."
}

$BuildPath = "$RepoPath/build"
$ResultsPath = "$RepoPath/test-results/unit"

function Test-Product([string]$Product, [string]$Suffix, [string]$ExcludeRegex) {
    $code = Invoke-ProductTests -BuildPath $BuildPath -Product $Product `
        -Configuration $Configuration -ExcludeRegex $ExcludeRegex `
        -JUnitPath "$ResultsPath/$Name-$Product$Suffix.xml"
    if ($code -ne 0) {
        Write-Host "Unit tests for '$Product$Suffix' failed with exit code $code"
        $script:failed = $code
    }
}

if ($Products -contains 'common') {
    Test-Product common '' '.*Performance|Integration|Example.*'
}

if ($Products -contains 'dd') {
    Test-Product dd '' '.*Example.*'
    Invoke-WithLiteOnly { Test-Product dd '-Lite' '.*Example.*' }
}

if ($Products -contains 'ipi') {
    $exclude = @('.*Example.*')
    if ($IsWindows) {
        # Every ctest entry is a separate process that reads the 2.3GB data
        # file before the test body runs, and Windows pays roughly twice what
        # Linux does for it. That read, not the test bodies, is what makes
        # this job slow: an EngineIpiTestsMemory test costs ~16s whether it
        # asserts one property or all of them. The Ubuntu jobs still run
        # everything below.
        #
        # EngineIpiTestsMemory*.Reload is the exception: it is skipped on
        # Linux (see EngineIpiTestsMemory.cpp), so Windows is its only
        # coverage. Keep it for the NullNull suite.
        $exclude += @(
            # Memory suites that only vary the required-properties filter.
            # The two Null ones are kept and exercise the same
            # init-from-memory path.
            'EngineIpiTestsMemory(InMemory|Null)(One|Two|Duplicate|Mixed|AllEdge)Property'
            'EngineIpiTestsMemoryInMemoryNull\.Reload'
            'EngineIpiTestsMemory.*\.(MetaDataReload|Size)'
            'EngineIpiTestsFile.*\.(Reload|MetaDataReload|Size)'
        )
    }
    Test-Product ipi '' ($exclude -join '|')
}

# One coverage report for the whole build, when it was built for coverage.
if (Test-Path "$BuildPath/CMakeFiles/*-cov.dir") {
    Push-Location $BuildPath
    try {
        Write-Output "Generating coverage report..."
        $artifacts = New-Item -ItemType directory -Path $RepoPath/artifacts -Force
        gcovr -r $RepoPath -o $artifacts/coverage.html --html-details --html-self-contained `
            --gcov-ignore-parse-errors=negative_hits.warn --exclude '.*\.hpp$' CMakeFiles
        if ($LASTEXITCODE -ne 0) { throw "gcovr failed" }
    } finally {
        Pop-Location
    }
}

exit $failed
