<#
.SYNOPSIS
Fails when a test file is missing from its Visual Studio test project.

.DESCRIPTION
CMake builds every .cpp and .hpp file in each test folder, while the Visual
Studio test projects list their files by name, so a test added to a folder is
left out of the Visual Studio build until it is added to the project. This
fails on any test file that its project, or the project's .filters file, does
not list.

main.cpp is not needed, because the googletest package supplies main.

.PARAMETER RepoPath
The root of the repository.
#>
param(
    [Parameter(Mandatory)][string]$RepoPath
)
$ErrorActionPreference = "Stop"

$RepoPath = (Resolve-Path $RepoPath).Path

# The Visual Studio project that builds the files in each test folder.
$common = 'VisualStudio/Common/Fiftyone.Common.Tests/Fiftyone.Common.Tests.vcxproj'
$hash = 'VisualStudio/DeviceDetection/FiftyOne.DeviceDetection.Hash.Tests/FiftyOne.DeviceDetection.Hash.Tests.vcxproj'
$ipi = 'VisualStudio/IpIntelligence/FiftyOne.IpIntelligence.Tests/FiftyOne.IpIntelligence.Tests.vcxproj'
$Projects = @{
    'test/common' = $common
    'test/dd' = $hash
    'test/dd/hash' = $hash
    'test/ipi' = $ipi
}

# Test folders with no Visual Studio project. test/both links both products
# into one program, test/common/performance holds a benchmark, and
# test/dd/tools runs a tool from a CMake script.
$NoProject = @('test/both', 'test/common/performance', 'test/dd/tools')

function Get-RelativePath([string]$Path) {
    [IO.Path]::GetRelativePath($RepoPath, $Path) -replace '\\', '/'
}

# Returns the files a project or filters file lists, as paths from the root of
# the repository. Visual Studio ignores case in paths, so this does too.
function Get-Listed([string]$ListPath) {
    $names = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $listFolder = Split-Path (Join-Path $RepoPath $ListPath)
    $xml = [xml](Get-Content -Raw -LiteralPath (Join-Path $RepoPath $ListPath))
    $items = $xml.SelectNodes(
        "//*[local-name()='ClCompile' or local-name()='ClInclude'][@Include]")
    foreach ($item in $items) {
        $path = Join-Path $listFolder ($item.GetAttribute('Include') -replace '\\', '/')
        [void]$names.Add((Get-RelativePath ([IO.Path]::GetFullPath($path))))
    }
    return , $names
}

$listed = @{}
foreach ($project in $Projects.Values | Sort-Object -Unique) {
    foreach ($list in $project, "$project.filters") {
        $listed[$list] = Get-Listed $list
    }
}

$problems = [Collections.Generic.List[string]]::new()
$checked = 0
$files = Get-ChildItem -LiteralPath (Join-Path $RepoPath 'test') -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.hpp' -and $_.Name -ne 'main.cpp' }
foreach ($file in $files) {
    $relative = Get-RelativePath $file.FullName
    $folder = $relative.Substring(0, $relative.LastIndexOf('/'))
    if ($NoProject | Where-Object { $folder -eq $_ -or $folder.StartsWith("$_/") }) {
        continue
    }
    $project = $Projects[$folder]
    if (!$project) {
        $problems.Add("$relative is in a folder with no Visual Studio test project. Add the folder to ci/$(Split-Path -Leaf $PSCommandPath).")
        continue
    }
    foreach ($list in $project, "$project.filters") {
        if (!$listed[$list].Contains($relative)) {
            $problems.Add("$relative is not in $list.")
        }
    }
    $checked++
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output $_ }
    exit 1
}
Write-Output "Each of the $checked test files is in its Visual Studio project and filters."
exit 0
