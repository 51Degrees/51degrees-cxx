
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$ProjectDir = ".",
    [string]$Name,
    [string]$Configuration = "Release",
    [string]$Arch = "x64"
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName, $ProjectDir, "build")

# Instead of calling the common CTest script, we want to exclude specific long
# running tests. Dropping the performance suites is not a coverage loss: they
# are run again by run-performance-tests.ps1, which is where their results are
# collected from. Match them by suite name rather than ".*Performance.*", which
# would also catch the HighPerformance configuration variants of every example.
$ExcludeRegex = @(
    '.*OfflineProcessing.*'
    'ExampleTestPerformance(Legacy)?\.'
)

if ($IsWindows) {
    # These exercise the file-backed collections, which are an order of
    # magnitude slower on Windows than on Linux - ExampleTestMem.LowMemory
    # alone is 260s here against 4s on Ubuntu. The three Ubuntu jobs still run
    # them, and ExampleTestReloadFromFile keeps its Default and Balanced cases.
    $ExcludeRegex += 'ExampleTestMem\.LowMemory'
    $ExcludeRegex += 'ExampleTestReloadFromFile\.(InMemory|LowMemory|HighPerformance)'
}

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    Write-Output "Testing $($Options.Name)"

    ctest -C $Configuration -T test --no-compress-output --output-on-failure --output-junit "../test-results/integration/$Name.xml" --tests-regex ".*Integration|Example.*" --exclude-regex ($ExcludeRegex -join '|')

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}

exit $LASTEXITCODE
