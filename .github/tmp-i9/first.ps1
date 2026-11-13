# Runs the same cases with several builds of the IP Intelligence performance
# example, the builds taking turns in every round so drift on the runner
# affects them all alike. Temporary, for issue 9's first table.
param(
    [Parameter(Mandatory)][string]$RepoPath,
    [Parameter(Mandatory)][string[]]$Builds, # name=path, the first is the base
    [Parameter(Mandatory)][string]$OutDir,
    [Parameter(Mandatory)][string]$Set,
    [int]$Rounds = 5,
    [double]$TargetSeconds = 3
)
$ErrorActionPreference = 'Stop'

$Data = Join-Path $RepoPath 'data/ip-intelligence/51Degrees-EnterpriseIpiV41.ipi'
$Evidence = Join-Path $RepoPath 'data/ip-intelligence/evidence.yml'
$Records = 20000
$Typical = 'Asn,AsnName,RegisteredName,RegisteredOwner,RegisteredCountry,' +
    'IpRangeStart,IpRangeEnd,Country,CountryCode,State,Town,Latitude,' +
    'Longitude,TimeZoneOffset,AccuracyRadiusMin,IsHosted,IsVPN,' +
    'CountryCodesGeographical,CountryCodesPopulation,Mcc'

$exes = [ordered]@{}
foreach ($build in $Builds) {
    $name, $path = $build -split '=', 2
    $exes[$name] = $path
}
$baseName = @($exes.Keys)[0]

function New-Case($Name, [string[]]$Arguments) {
    [pscustomobject]@{ Name = $Name; Arguments = $Arguments }
}
function Preset($PresetName, $Properties, $Read, $Threads) {
    @('--profile', $PresetName, '--properties', $Properties, '--read', $Read,
        '--threads', "$Threads")
}
function Balanced($Properties, $Read, $Threads) {
    Preset Balanced $Properties $Read $Threads
}
$cases = switch ($Set) {
    'first' { @(
        foreach ($preset in 'Balanced', 'InMemory') {
            foreach ($threads in 1, 4) {
                $t = $threads -eq 1 ? '1 thread' : "$threads threads"
                New-Case "$preset, 20 typical properties, find only, $t" (Preset $preset $Typical none $threads)
                New-Case "$preset, every property as text, $t" (Preset $preset all text $threads)
                New-Case "$preset, 20 typical properties as text, $t" (Preset $preset $Typical text $threads)
            }
        }
    ) }
    'named' { @(
        foreach ($preset in 'HighPerformance', 'Balanced') {
            foreach ($threads in 1, 4) {
                $t = $threads -eq 1 ? '1 thread' : "$threads threads"
                New-Case "$preset, 20 typical properties, find only, $t" (Preset $preset $Typical none $threads)
                New-Case "$preset, 20 typical properties as text, $t" (Preset $preset $Typical text $threads)
            }
        }
    ) }
    default { throw "No set named '$Set'" }
}

function Invoke-Example([string]$Exe, [string[]]$Arguments, [int]$Passes) {
    $json = Join-Path $OutDir 'last.json'
    Remove-Item $json -ErrorAction SilentlyContinue
    $all = @('--data-file', $Data, '--ip-addresses-file', $Evidence,
        '--json-output', $json, '--iterations', "$Records",
        '--passes', "$Passes") + $Arguments
    Push-Location $RepoPath
    try {
        $text = (& $Exe @all) -join "`n"
        if ($LASTEXITCODE -ne 0) {
            Write-Host $text
            throw "The example exited with $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
    $figures = (Get-Content $json -Raw | ConvertFrom-Json -AsHashtable).Values |
        Select-Object -First 1
    [pscustomobject]@{
        Rate = [double]$figures.LookupsPerSecond
        Checksum = [regex]::Match($text, 'Checksum: (\d+)').Groups[1].Value
    }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
$rows = [System.Collections.Generic.List[object]]::new()
$os = $IsWindows ? 'Windows' : 'Linux'
foreach ($case in $cases) {
    $first = Invoke-Example $exes[$baseName] $case.Arguments 1
    $passes = [int][Math]::Max(1, [Math]::Ceiling($TargetSeconds * $first.Rate / $Records))
    for ($round = 1; $round -le $Rounds; $round++) {
        foreach ($name in $exes.Keys) {
            $run = Invoke-Example $exes[$name] $case.Arguments $passes
            $rows.Add([pscustomobject]@{ System = $os; Case = $case.Name; Build = $name;
                Round = $round; Passes = $passes; Rate = $run.Rate; Checksum = $run.Checksum })
            Write-Host ("{0,-56} {1,-8} round {2}: {3,10:N0} lookups/s, checksum {4}" -f
                $case.Name, $name, $round, $run.Rate, $run.Checksum)
        }
    }
}
$rows | Export-Csv -NoTypeInformation (Join-Path $OutDir "turns-$Set-$os.csv")

$lines = @("## $os, $Set, builds taking turns, middle of $Rounds rounds", '',
    '| Case | Build | Lookups a second | Lowest | Highest | Against base | Checksums |',
    '| --- | --- | ---: | ---: | ---: | ---: | --- |')
foreach ($group in ($rows | Group-Object Case)) {
    $baseRows = @($group.Group | Where-Object Build -eq $baseName | Sort-Object Rate)
    $base = $baseRows[[int][Math]::Floor($baseRows.Count / 2)].Rate
    foreach ($build in ($group.Group | Group-Object Build)) {
        $sorted = @($build.Group | Sort-Object Rate)
        $middle = $sorted[[int][Math]::Floor($sorted.Count / 2)].Rate
        $lines += "| $($group.Name) | $($build.Name) | $('{0:N0}' -f $middle) | $('{0:N0}' -f $sorted[0].Rate) | $('{0:N0}' -f $sorted[-1].Rate) | $('{0:N2}' -f ($middle / $base)) | $((@($build.Group.Checksum | Select-Object -Unique)) -join ' ') |"
    }
}
$lines | Set-Content (Join-Path $OutDir "turns-$Set-$os.md")
if ($env:GITHUB_STEP_SUMMARY) { $lines | Add-Content $env:GITHUB_STEP_SUMMARY }
$lines | Write-Host

# Peak memory on Linux, from one pass of each case and build under GNU time.
if ($IsLinux) {
    $memory = @('', "## Linux, $Set, peak resident memory of one pass", '',
        '| Case | Build | Peak MB |', '| --- | --- | ---: |')
    foreach ($case in $cases) {
        foreach ($name in $exes.Keys) {
            $log = Join-Path $OutDir 'time.txt'
            $all = @('-v', '-o', $log, $exes[$name], '--data-file', $Data,
                '--ip-addresses-file', $Evidence, '--iterations', "$Records",
                '--passes', '1') + $case.Arguments
            Push-Location $RepoPath
            try { & /usr/bin/time @all | Out-Null } finally { Pop-Location }
            $kb = [regex]::Match((Get-Content $log -Raw),
                'Maximum resident set size \(kbytes\): (\d+)').Groups[1].Value
            $memory += "| $($case.Name) | $name | $('{0:N0}' -f ([double]$kb / 1024)) |"
        }
    }
    $memory | Add-Content (Join-Path $OutDir "turns-$Set-$os.md")
    if ($env:GITHUB_STEP_SUMMARY) { $memory | Add-Content $env:GITHUB_STEP_SUMMARY }
    $memory | Write-Host
}
