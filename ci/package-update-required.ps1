param (
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [Parameter(Mandatory=$true)]
    [string]$Version
)

# Exit 0 when a release is needed and 1 when not, as the publish workflow
# expects. A release is the tag the language repositories take as a
# submodule, so a change that adds nothing they build from is not an update.
# That is Markdown, .github, and any docs, examples, test or tests folder or
# LICENSE file at any depth, plus the data submodules at the root, which
# tests and examples fetch and no language builds from.
Write-Output "Checking if update is required"
git -C $RepoName show-ref --quiet --tags $Version

if ($LASTEXITCODE -eq 0) {
    Write-Output "Version '$Version' already present, no update needed"
    exit 1
}

# Without a release tag there is nothing to compare with.
$lastTag = git -C $RepoName describe --tags --abbrev=0 2>$null
if ($LASTEXITCODE -eq 0 -and $lastTag) {
    $notPackage = @(
        '\.md$',
        '^\.github/',
        '(^|/)(docs|examples|test|tests)/',
        '(^|/)LICENSE[^/]*$',
        '^(data|device-detection-data|ip-intelligence-data)(/|$)'
    )
    $changed = @(git -C $RepoName diff --name-only $lastTag HEAD)
    $packageFiles = @($changed | Where-Object {
        $file = $_
        -not ($notPackage | Where-Object { $file -imatch $_ })
    })
    if ($changed.Count -gt 0 -and $packageFiles.Count -eq 0) {
        Write-Output "Only files outside the package changed since $lastTag, no update needed"
        exit 1
    }
}

exit 0
