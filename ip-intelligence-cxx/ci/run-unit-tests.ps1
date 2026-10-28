param(
    [string]$RepoName,
    [string]$ProjectDir = ".",
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake"
)

$ExcludeRegex = ".*Example.*"

if ($IsWindows) {
    # Every ctest entry is a separate process that reads the 2.3GB data file
    # before the test body runs, and Windows pays roughly twice what Linux does
    # for it. That read, not the test bodies, is what makes this job slow: an
    # EngineIpiTestsMemory test costs ~16s whether it asserts one property or
    # all of them. The three Ubuntu jobs still run everything below.
    #
    # EngineIpiTestsMemory*.Reload is the exception: it is skipped on Linux
    # (see EngineIpiTestsMemory.cpp), so Windows is its only coverage. Keep it
    # for the NullNull suite.
    $LongRunningOnWindows = @(
        # Memory suites that only vary the required-properties filter. The two
        # Null ones are kept and exercise the same init-from-memory path.
        'EngineIpiTestsMemory(InMemory|Null)(One|Two|Duplicate|Mixed|AllEdge)Property'
        'EngineIpiTestsMemoryInMemoryNull\.Reload'
        'EngineIpiTestsMemory.*\.(MetaDataReload|Size)'
        'EngineIpiTestsFile.*\.(Reload|MetaDataReload|Size)'
    )
    $ExcludeRegex = (@($ExcludeRegex) + $LongRunningOnWindows) -join '|'
}

./cxx/run-unit-tests.ps1 `
    -RepoName $RepoName `
    -ProjectDir $ProjectDir `
    -Name $Name `
    -Configuration $Configuration `
    -Arch $Arch `
    -BuildMethod $BuildMethod `
    -ExcludeRegex $ExcludeRegex `
    -CoverageExcludeDirs 'fiftyone-common-c(xx)?-cov\.dir$'

exit $LASTEXITCODE
