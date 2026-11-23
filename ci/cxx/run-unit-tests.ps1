<#
.SYNOPSIS
Runs the test programs of one Visual Studio build.

.DESCRIPTION
Called by ci/run-unit-tests.ps1, from the folder that holds the clone of the
repository. Runs every program in the build folder whose name contains Test,
each of which is a GoogleTest program, and writes its results to
test-results/unit in the repository.

Each program runs under the console debugger, cdb, from the Debugging Tools
for Windows, when the runner has the one for the build's architecture. When a
program crashes in any thread, the debugger writes a full memory dump, the log
shows the stack of every thread, and the dump, the program and its PDB file
are put in artifacts/crash-dumps in the repository. Windows Error Reporting
writes no dumps on the hosted runners, which is why a debugger is used. The
debugger lets the crash go on once the dump is written, so the program's exit
code is the same as it is without the debugger.

.PARAMETER ProjectDir
The folder of the solution, relative to the repository.

.PARAMETER BuildDir
The folder the build wrote to, relative to the solution folder.
#>
param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$ProjectDir = ".",
    [string]$Name,
    [string]$Configuration = "Release",
    [string]$Arch = "x64",
    [string]$BuildMethod = "msbuild",
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

if ($BuildMethod -ne "msbuild") {
    Write-Error "The build method '$BuildMethod' is not supported."
}

$RepoPath = "$PWD/$RepoName"
$BuildPath = "$RepoPath/$ProjectDir/$BuildDir"
$ArtifactPath = "$RepoPath/artifacts/crash-dumps/$Name"

# Exceptions that end the program when it does not handle them: access
# violation, assertion, breakpoint, integer divide by zero, illegal
# instruction, integer overflow, in-page error, guard page, stack overflow,
# fail fast (which abort and heap corruption use), and every exception code
# the debugger has no name for.
$CrashEvents = @('av', 'asrt', 'bpe', 'dz', 'ii', 'iov', 'ip', 'gp', 'sov',
    'sbo', '*')

# Returns the cdb for the build's architecture, or nothing when the runner
# does not have it.
function Get-Cdb([string]$Arch) {
    if ($Arch -notin @('x86', 'x64') -or !${env:ProgramFiles(x86)}) {
        return $null
    }
    $cdb = Join-Path ${env:ProgramFiles(x86)} "Windows Kits/10/Debuggers/$Arch/cdb.exe"
    if (Test-Path $cdb) { return $cdb }
    return $null
}

# Runs one test program under the debugger, leaving its exit code in
# $LASTEXITCODE. The debugger stops only on an exception the program does not
# handle, so the exceptions GoogleTest catches pass through as they do without
# it. On such an exception it writes the dump, then lets the exception go on
# unhandled (gn), so the program ends with the exception code as it would on
# its own. The debug heap is turned off (-hd) so the heap behaves as it does
# without the debugger.
function Invoke-UnderDebugger([string]$Cdb, [string]$Program,
    [string[]]$Arguments, [string]$DumpFile) {
    $onCrash = ".dump /ma /u $DumpFile; gn"
    $commands = @('sxi eh', 'sxi ld') +
        ($CrashEvents | ForEach-Object { "sxd -c2 `"$onCrash`" $_" }) + 'g'
    $PSNativeCommandUseErrorActionPreference = $false
    & $Cdb -G -hd -y $BuildFolder -c ($commands -join '; ') $Program @Arguments
}

# Prints the stack of every thread in the dump, then the stack of the thread
# that faulted, at the exception. Symbols come from the build folder and, for
# the Windows libraries, from the Microsoft symbol server.
function Show-CrashDump([string]$Cdb, [string]$Dump) {
    $cache = Join-Path $DumpRoot 'symbols'
    $symbols = "$BuildFolder;srv*$cache*https://msdl.microsoft.com/download/symbols"
    $commands = '.echo Stacks of every thread; ~*kb; ' +
        '.echo Stack of the faulting thread at the exception; .ecxr; kb; q'
    $PSNativeCommandUseErrorActionPreference = $false
    & $Cdb -z $Dump -y $symbols -lines -c $commands | Out-Host
}

Write-Output "Entering '$BuildPath'"
Push-Location $BuildPath
try {
    $BuildFolder = (Get-Location).Path
    $TestBinaries = Get-ChildItem -Filter *Test*.exe

    # Dumps are written to the temporary folder and moved to the artifacts
    # only after a crash, so a run that passes leaves nothing behind. The
    # debugger takes the dump path inside a quoted command, so the path is
    # passed with forward slashes and must hold no spaces.
    $Cdb = Get-Cdb $Arch
    $DumpRoot = $env:RUNNER_TEMP ? $env:RUNNER_TEMP : [IO.Path]::GetTempPath()
    $DumpDir = (Join-Path $DumpRoot "crash-dumps/$Name") -replace '\\', '/'
    if (!$Cdb) {
        Write-Output "No debugger for '$Arch' on this runner, so a crash leaves no dump"
    } elseif ($DumpDir -match '\s') {
        Write-Output "The dump folder '$DumpDir' has a space, so a crash leaves no dump"
        $Cdb = $null
    } else {
        New-Item -ItemType Directory -Path $DumpDir -Force | Out-Null
    }

    foreach ($TestBinary in $TestBinaries) {
        Write-Output $TestBinary.FullName
        Write-Output "Testing $Name-$($TestBinary.Name)"
        $XmlOutPath = "$RepoPath\test-results\unit\${Name}_$($TestBinary.BaseName).xml"
        $arguments = @('--gtest_catch_exceptions=1', '--gtest_break_on_failure=0',
            "--gtest_output=xml:$XmlOutPath")
        if (!$Cdb) {
            & $TestBinary.FullName @arguments
        } else {
            Invoke-UnderDebugger -Cdb $Cdb -Program $TestBinary.FullName `
                -Arguments $arguments -DumpFile "$DumpDir/$($TestBinary.BaseName).dmp"
            $code = $LASTEXITCODE
            if ($code -ne 0) {
                $dumps = @(Get-ChildItem $DumpDir -Filter "$($TestBinary.BaseName)*.dmp")
                foreach ($dump in $dumps) {
                    Show-CrashDump -Cdb $Cdb -Dump $dump.FullName
                }
                if ($dumps) {
                    New-Item -ItemType Directory -Path $ArtifactPath -Force | Out-Null
                    $dumps | Move-Item -Destination $ArtifactPath
                    $pdb = "$($TestBinary.BaseName).pdb"
                    Copy-Item $TestBinary.FullName $ArtifactPath
                    if (Test-Path $pdb) { Copy-Item $pdb $ArtifactPath }
                    Write-Output "The dump, the program and its PDB file are in '$ArtifactPath'"
                }
                throw "Program `"$($TestBinary.Name)`" ended with non-zero exit code: $code ($('0x{0:X8}' -f $code))."
            }
        }
        Write-Output "Results saved to $XmlOutPath"
    }

} finally {
    Write-Output "Leaving '$BuildPath'"
    Pop-Location
}
