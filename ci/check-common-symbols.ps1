<#
.SYNOPSIS
Fails when Device Detection and IP Intelligence could bind to each other's copy
of the common C library.

.DESCRIPTION
Each product links its own copy of common, compiled with different structure
sizes. src/common/ipiSymbols.h renames the IP Intelligence copy with an ipi_
prefix. A name missing from that list would be exported under its original name
by both copies, and the linker would bind every call to just one of them, so one
product would read its data through the other product's code.

This reads the symbols a build left in the build folder and fails on any of:

- a name the IP Intelligence copy of common defines without the ipi_ prefix;
- a name both products define;
- a fiftyoneDegrees name IP Intelligence references but does not define, which
  only the Device Detection copy then provides;
- a common header that does not include the list first.

It uses nm on Linux and macOS, which sees every external name, and dumpbin on
Windows, where only the 51Degrees names are read because each object also
carries the C runtime's inline functions as external names.

.PARAMETER BuildPath
The CMake build folder of a build with FIFTYONE_IPI_SYMBOLS on.
#>
param(
    [Parameter(Mandatory)][string] $BuildPath
)

$ErrorActionPreference = 'Stop'

$ListHeader = 'ipiSymbols.h'
$CommonSource = Join-Path (Split-Path -Parent $PSScriptRoot) 'src/common'

function Find-Library([string] $Name) {
    $found = Get-ChildItem -Path (Join-Path $BuildPath 'lib') -Recurse -File |
        Where-Object { $_.Name -match "^(lib)?$([regex]::Escape($Name))\.(a|lib)$" } |
        Select-Object -First 1
    if ($found) { $found.FullName }
}

function Find-Dumpbin {
    $onPath = Get-Command dumpbin -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -find 'VC/Tools/MSVC/**/bin/Hostx64/x64/dumpbin.exe' |
            Select-Object -First 1
        if ($found) { return $found }
    }
    throw 'dumpbin was not found on the PATH or through vswhere'
}

# Returns @{ Defined = names; Undefined = names } across the libraries.
function Get-Symbols([string[]] $Libraries) {
    $defined = [System.Collections.Generic.HashSet[string]]::new()
    $undefined = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($library in $Libraries) {
        if ($IsWindows) {
            foreach ($line in & (Find-Dumpbin) /SYMBOLS $library) {
                if ($line -match 'External\s+\|\s+(\S*fiftyoneDegrees\S*)') {
                    if ($line -match '\bUNDEF\b') { [void]$undefined.Add($Matches[1]) }
                    else { [void]$defined.Add($Matches[1]) }
                }
            }
        } else {
            foreach ($line in & nm -g $library 2>$null) {
                $fields = -split $line
                if ($fields.Count -lt 2) { continue }
                # macOS prefixes C names with an underscore.
                $name = $IsMacOS ? ($fields[-1] -replace '^_', '') : $fields[-1]
                if ($fields[-2] -eq 'U') { [void]$undefined.Add($name) }
                else { [void]$defined.Add($name) }
            }
        }
    }
    if ($defined.Count -eq 0) { throw "no symbols read from $($Libraries -join ', ')" }
    @{ Defined = $defined; Undefined = $undefined }
}

# Returns the paths of the libraries that exist, requiring the first.
function Get-Libraries([string[]] $Names) {
    $paths = @($Names | ForEach-Object { Find-Library $_ })
    if (-not (Find-Library $Names[0])) {
        throw "no $($Names[0]) library under $BuildPath/lib, so build it first"
    }
    $paths
}

$problems = [System.Collections.Generic.List[string]]::new()

foreach ($header in Get-ChildItem -Path $CommonSource -Filter *.h) {
    if ($header.Name -eq $ListHeader) { continue }
    $first = Select-String -Path $header.FullName -Pattern '^\s*#\s*include\b' |
        Select-Object -First 1
    if (-not $first -or $first.Line -notmatch [regex]::Escape($ListHeader)) {
        $problems.Add("$($header.Name) does not include $ListHeader first.")
    }
}

$ipiCommon = Get-Symbols (Get-Libraries @('fiftyone-common-c-ipi'))
$dd = Get-Symbols (Get-Libraries @(
    'fiftyone-common-c', 'fiftyone-device-detection-c', 'fiftyone-hash-c'))
$ipi = Get-Symbols (Get-Libraries @(
    'fiftyone-common-c-ipi', 'fiftyone-ip-intelligence-c'))

foreach ($name in $ipiCommon.Defined | Where-Object { $_ -notlike 'ipi_*' } | Sort-Object) {
    $problems.Add("IP Intelligence's copy of common defines $name without the prefix. Add it to src/common/$ListHeader.")
}
foreach ($name in $ipi.Defined | Where-Object { $dd.Defined.Contains($_) } | Sort-Object) {
    $problems.Add("Both products define $name.")
}
$borrowed = $ipi.Undefined | Where-Object {
    $_ -like 'fiftyoneDegrees*' -and -not $ipi.Defined.Contains($_) -and $dd.Defined.Contains($_)
} | Sort-Object
foreach ($name in $borrowed) {
    $problems.Add("IP Intelligence calls the Device Detection copy of $name. Add it to src/common/$ListHeader.")
}

Write-Output ("Device Detection defines {0} names, IP Intelligence defines {1}, {2} of them from its copy of common." -f `
    $dd.Defined.Count, $ipi.Defined.Count, $ipiCommon.Defined.Count)
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output $_ }
    exit 1
}
Write-Output 'No name is shared between the two products.'
exit 0
