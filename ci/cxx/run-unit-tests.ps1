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
program crashes or aborts in a way GoogleTest does not catch, in any thread,
the debugger writes a dump to the runner's temporary folder, the log shows the
stack of every thread from it, and the dump is then deleted. The debugger lets
the crash go on once the dump is written, so the program fails as it does
without the debugger.

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

# Exceptions that end the program when it does not handle them: access
# violation, assertion, breakpoint, integer divide by zero, illegal
# instruction, integer overflow, in-page error, guard page, stack overflow,
# fail fast, and every exception code the debugger has no name for.
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
# unhandled (gn), so the program still fails. The exit code is the exception
# code, as without the debugger, except for a breakpoint, which an x86 program
# under a debugger ends with as 0x4000001F rather than 0x80000003, and a C++
# exception that leaves a thread not started with std::thread, such as one
# started with CreateThread, which ends the program with 0xE06D7363 rather than
# with exit code 3 from abort.
#
# GoogleTest makes abort end the program with exit code 3 and no exception, so
# std::terminate, which an exception leaving a std::thread reaches, would leave
# no dump. A breakpoint on abort, in the release and the debug C runtime,
# writes the dump, in which the thread that threw is still at the throw, then
# lets abort go on.
#
# Three settings keep the program behaving as it does without a debugger.
# Breakpoints are passed to the program rather than handled by the debugger
# (sxd bpec), so they end it as they would on their own. Closing a handle that
# is not valid raises an exception only under a debugger, so that exception is
# ignored and marked handled (sxi ch, sxe hc), and CloseHandle just fails. The
# debug heap is turned off (-hd).
function Invoke-UnderDebugger([string]$Cdb, [string]$Program,
    [string[]]$Arguments, [string]$DumpFile) {
    $onCrash = ".dump /ma /u $DumpFile; gn"
    $onAbort = ".dump /ma /u $DumpFile; g"
    $commands = @('sxi ld', 'sxd bpec', 'sxi ch', 'sxe hc',
        "sxi -c2 `"$onCrash`" eh",
        "bu ucrtbase!abort `"$onAbort`"", "bu ucrtbased!abort `"$onAbort`"") +
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

    # A dump holds the whole of the program's memory, so it stays in the
    # runner's temporary folder and is deleted once its stacks are printed.
    # The debugger takes the dump path inside a quoted command, so the path is
    # passed with forward slashes and must hold no spaces.
    $Cdb = Get-Cdb $Arch
    $DumpRoot = $env:RUNNER_TEMP ? $env:RUNNER_TEMP : [IO.Path]::GetTempPath()
    $DumpDir = (Join-Path $DumpRoot "crash-dumps/$Name") -replace '\\', '/'
    if (!$Cdb) {
        Write-Output "No debugger for '$Arch' on this runner, so a crash prints no stacks"
    } elseif ($DumpDir -match '\s') {
        Write-Output "The dump folder '$DumpDir' has a space, so a crash prints no stacks"
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
                $dumps = @(Get-ChildItem $DumpDir -Filter "$($TestBinary.BaseName)_*.dmp")
                foreach ($dump in $dumps) {
                    Show-CrashDump -Cdb $Cdb -Dump $dump.FullName
                    Remove-Item $dump.FullName
                }
                # cdb ends with this code when it stops on an event it has no
                # command for and finds no input.
                if (!$dumps -and $code -eq [int]0x8007006D) {
                    Write-Output "The debugger stopped on an event it was not set up for, so this exit code is the debugger's"
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
