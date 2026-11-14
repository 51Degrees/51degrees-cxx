<#
.SYNOPSIS
Compiles every source file and every header under src with the compiler's
default language standard and warnings as errors.

.DESCRIPTION
The languages that build this library compile its sources with their own
build tools, which use the compiler's default standard and often make
warnings errors. The CMake build sets C11 and C++17, so it cannot show a
warning that only a newer default standard gives. Each file is compiled as
Device Detection builds it and, for common and IP Intelligence, as IP
Intelligence builds it, with -Wall -Werror and the optimisation a release
build uses, as some warnings need it. Each header is then compiled on its
own, because a file that includes a header first has nothing before it to
bring in what the header left out. Runs on Linux with gcc and g++.

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

function Test-File([string]$Compiler, [string[]]$Options, [string]$File,
    [string]$Name, [string[]]$Definitions, [string]$As) {
    $output = & $Compiler -Wall -Werror @knownWarnings @Options $File `
        @Definitions 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:failed += "$Name $As"
        Write-Output "$Name does not compile $As"
        $output | Select-Object -First 12 | ForEach-Object { "  $_" }
    }
}

function Get-Name($File) {
    [IO.Path]::GetRelativePath($RepoPath, $File.FullName).Replace('\', '/')
}

$files = @(Get-ChildItem -Recurse -File "$RepoPath/src" |
    Where-Object { $_.Extension -in '.c', '.cpp' } |
    Sort-Object FullName)
$build = '-O2', '-c', '-o', '/dev/null'
foreach ($file in $files) {
    $name = Get-Name $file
    $compiler = $file.Extension -eq '.c' ? 'gcc' : 'g++'
    if ($name -notmatch '^src/ipi/') {
        Test-File $compiler $build $file.FullName $name @() `
            'as Device Detection'
    }
    if ($name -notmatch '^src/dd/') {
        Test-File $compiler $build $file.FullName $name $ipi `
            'as IP Intelligence'
    }
}

$headers = @(Get-ChildItem -Recurse -File "$RepoPath/src" |
    Where-Object { $_.Extension -in '.h', '.hpp' } |
    Sort-Object FullName)
foreach ($file in $headers) {
    $name = Get-Name $file
    $c = $file.Extension -eq '.h'
    $options = '-x', ($c ? 'c' : 'c++'), '-fsyntax-only'
    $definitions = $name -match '^src/ipi/' ? $ipi : @()
    Test-File ($c ? 'gcc' : 'g++') $options $file.FullName $name `
        $definitions 'on its own'
}

if ($script:failed) {
    Write-Output ("$($script:failed.Count) compilations of " +
        "$($files.Count) source files and $($headers.Count) headers failed")
    exit 1
}
Write-Output ("All $($files.Count) source files and $($headers.Count) " +
    "headers compile with the default standard and no warnings")
