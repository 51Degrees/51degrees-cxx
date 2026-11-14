<#
.SYNOPSIS
Compiles every source file under src with the compiler's default language
standard and warnings as errors.

.DESCRIPTION
The languages that build this library compile its sources with their own
build tools, which use the compiler's default standard and often make
warnings errors. The CMake build sets C11 and C++17, so it cannot show a
warning that only a newer default standard gives. Each file is compiled as
Device Detection builds it and, for common and IP Intelligence, as IP
Intelligence builds it, with -Wall -Werror and the optimisation a release
build uses, as some warnings need it. Runs on Linux with gcc and g++.

The sources give five kinds of warning under every standard, which are turned
off here, so that the check shows what a new default standard adds.

.PARAMETER RepoPath
The root of the repository.
#>
param(
    [string]$RepoPath = "$PSScriptRoot/.."
)
$ErrorActionPreference = 'Stop'

$RepoPath = (Resolve-Path $RepoPath).Path

# The definitions the IP Intelligence copy of common is compiled with.
$ipi = @(
    '-DFIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT'
    '-DFIFTYONE_DEGREES_REDUCED_FILE'
)

# Warnings the sources give under every standard.
$knownWarnings = @(
    '-Wno-missing-braces'
    '-Wno-unused-function'
    '-Wno-unused-variable'
    '-Wno-unused-but-set-variable'
    '-Wno-strict-aliasing'
)

gcc --version | Select-Object -First 1
g++ --version | Select-Object -First 1

$script:failed = @()

function Test-File([string]$Compiler, [string]$File, [string]$Name,
    [string[]]$Definitions, [string]$Product) {
    $output = & $Compiler -Wall -Werror @knownWarnings -O2 -c $File `
        -o /dev/null @Definitions 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:failed += "$Name as $Product"
        Write-Output "$Name does not compile as $Product"
        $output | Select-Object -First 12 | ForEach-Object { "  $_" }
    }
}

$files = @(Get-ChildItem -Recurse -File "$RepoPath/src" |
    Where-Object { $_.Extension -in '.c', '.cpp' } |
    Sort-Object FullName)
foreach ($file in $files) {
    $name = [IO.Path]::GetRelativePath($RepoPath, $file.FullName) -replace '\\', '/'
    $compiler = $file.Extension -eq '.c' ? 'gcc' : 'g++'
    if ($name -notmatch '^src/ipi/') {
        Test-File $compiler $file.FullName $name @() 'Device Detection'
    }
    if ($name -notmatch '^src/dd/') {
        Test-File $compiler $file.FullName $name $ipi 'IP Intelligence'
    }
}

if ($script:failed) {
    Write-Output "$($script:failed.Count) compilations of $($files.Count) source files failed"
    exit 1
}
Write-Output "All $($files.Count) source files compile with the default standard and no warnings"
