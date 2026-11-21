param(
    # Every entry of options.json.
    [Parameter(Mandatory)][object[]]$Options,
    # The files the pull request changes, less documentation.
    [string[]]$ChangedFiles,
    # Those of the changed files whose change touches only comments.
    [string[]]$CommentOnlyFiles
)
# Chooses the options.json entries a pull request is built on, for common-ci's
# steps/select-options.ps1. The branch itself and a release build every entry.
#
# The standard set builds each product the change touches with GCC and MSVC,
# and runs its tests, less the IP Intelligence ones, which read the 2.3GB
# Enterprise data file. Those run when an IP Intelligence file changes in more
# than its comments. A change to a product's source in more than its comments
# adds every other entry that builds the product, and a change to a file no
# product owns, such as common, CMake or CI, counts for every product.
# Performance is measured by the branch and release builds only.
. $PSScriptRoot/products.ps1

$Standard = @(
    'Ubuntu_x64_Release'
    'Windows_VS_x64_Release'
    'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly'
    'Windows_VS_IPI_x64_Release_BuildOnly'
)

# Left to the branch and release builds. The Debug builds are repeated by
# another entry, Ubuntu_x64_Debug by the sanitizer build and Ubuntu_x86_Debug
# by Ubuntu_x86_Release. Only five macOS jobs run at once across the
# organisation, and the releases need them.
$FullOnly = @('Ubuntu_x64_Debug', 'Ubuntu_x86_Debug', 'Mac_arm64_Debug')

# A product's source. Its other paths hold tests, examples, tools and data.
$SourcePaths = @{
    dd = @('src/dd/', 'VisualStudio/DeviceDetection/')
    ipi = @('src/ipi/', 'VisualStudio/IpIntelligence/')
}

$code = @($ChangedFiles | Where-Object { $CommentOnlyFiles -notcontains $_ })
$touched = @(Resolve-AffectedProducts $ChangedFiles)
$tested = $code ? @(Resolve-AffectedProducts $code) : @()

# The products whose source changes in more than its comments.
$source = @()
foreach ($path in $code) {
    $owners = @(Resolve-AffectedProducts @($path))
    if ($owners.Count -ne 1) {
        $source = $AllProducts
        break
    }
    $isSource = @($SourcePaths[$owners[0]] |
        Where-Object { $path.StartsWith($_) }).Count -gt 0
    if ($isSource -and $source -notcontains $owners[0]) {
        $source += $owners[0]
    }
}
Write-Host ("Touched: $($touched -join ', '). Tested: $($tested -join ', '). " +
    "Source: $($source -join ', ').")

foreach ($entry in $Options) {
    $requested = $entry.Products ? $entry.Products : $AllProducts
    if ($FullOnly -contains $entry.Name) {
        Write-Host "$($entry.Name) is left to the branch and release builds"
        continue
    }
    if ($Standard -contains $entry.Name) {
        $products = @($requested | Where-Object { $touched -contains $_ })
        $entry.Depth = ($products -contains 'ipi' -and $tested -contains 'ipi') ?
            'deep' : 'standard'
    } else {
        $products = @($requested | Where-Object { $source -contains $_ })
        # A program linking both products tells nothing with only one.
        if ($entry.CxxBothProducts -and $products.Count -lt 2) {
            $products = @()
        }
        $entry.Depth = 'deep'
    }
    if (!$products) {
        Write-Host "$($entry.Name) builds nothing this change touches"
        continue
    }
    $entry.Products = $products
    $entry.RunPerformance = $false
    Write-Host "$($entry.Name) builds $($products -join ', ') at depth $($entry.Depth)"
    $entry
}
