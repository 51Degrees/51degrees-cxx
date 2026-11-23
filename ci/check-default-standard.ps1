<#
.SYNOPSIS
Compiles every source file and every header under src with the compiler's
default language standard and warnings as errors.

.DESCRIPTION
The languages that build this library compile its sources with their own
build tools, which use the compiler's default standard and their own warning
options, such as -Wall -Wextra in node-gyp. The CMake build sets C11 and
C++17, so it cannot show a warning that only a newer default standard gives.
Each file is compiled as Device Detection builds it and, for common and IP
Intelligence, as IP Intelligence builds it, with -Wall -Wextra -Werror and
the optimisation a release build uses, as some warnings need it. Each header
is then compiled on its own, because a file that includes a header first has
nothing before it to bring in what the header left out. Runs on Linux with
gcc and g++, then with clang and clang++, as each gives warnings the other
does not.

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

# The C and C++ compiler of each toolchain.
$toolchains = @(
    @{ C = 'gcc'; Cxx = 'g++' }
    @{ C = 'clang'; Cxx = 'clang++' }
)

$script:failed = @()

function Test-File([string]$Compiler, [string[]]$Options, [string]$File,
    [string]$Name, [string[]]$Definitions, [string]$As) {
    $output = & $Compiler -Wall -Wextra -Werror @Options $File `
        @Definitions 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:failed += "$Name $As with $Compiler"
        Write-Output "$Name does not compile $As with $Compiler"
        $output | Select-Object -First 12 | ForEach-Object { "  $_" }
    }
}

function Get-Name($File) {
    [IO.Path]::GetRelativePath($RepoPath, $File.FullName).Replace('\', '/')
}

$files = @(Get-ChildItem -Recurse -File "$RepoPath/src" |
    Where-Object { $_.Extension -in '.c', '.cpp' } |
    Sort-Object FullName)
$headers = @(Get-ChildItem -Recurse -File "$RepoPath/src" |
    Where-Object { $_.Extension -in '.h', '.hpp' } |
    Sort-Object FullName)
$build = '-O2', '-c', '-o', '/dev/null'

foreach ($toolchain in $toolchains) {
    & $toolchain.C --version | Select-Object -First 1
    & $toolchain.Cxx --version | Select-Object -First 1

    foreach ($file in $files) {
        $name = Get-Name $file
        $compiler = $file.Extension -eq '.c' ? $toolchain.C : $toolchain.Cxx
        if ($name -notmatch '^src/ipi/') {
            Test-File $compiler $build $file.FullName $name @() `
                'as Device Detection'
        }
        if ($name -notmatch '^src/dd/') {
            Test-File $compiler $build $file.FullName $name $ipi `
                'as IP Intelligence'
        }
    }

    foreach ($file in $headers) {
        $name = Get-Name $file
        $c = $file.Extension -eq '.h'
        $options = '-x', ($c ? 'c' : 'c++'), '-fsyntax-only'
        $definitions = $name -match '^src/ipi/' ? $ipi : @()
        Test-File ($c ? $toolchain.C : $toolchain.Cxx) $options `
            $file.FullName $name $definitions 'on its own'
    }
}

if ($script:failed) {
    Write-Output ("$($script:failed.Count) compilations of " +
        "$($files.Count) source files and $($headers.Count) headers failed")
    exit 1
}
Write-Output ("All $($files.Count) source files and $($headers.Count) " +
    "headers compile with the default standard and no warnings")
