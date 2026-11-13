# Times the performance examples of two builds on one runner, the builds
# taking turns in every round, as CI's performance step runs them. Temporary,
# for the release of rc into main.
param(
    [Parameter(Mandatory)][string]$RepoPath,
    [Parameter(Mandatory)][string[]]$Builds, # name=bin folder, the first is the base
    [Parameter(Mandatory)][string]$OutDir,
    [int]$Rounds = 5
)
$ErrorActionPreference = 'Stop'
$exe = $IsWindows ? '.exe' : ''
$data = Join-Path $RepoPath 'data'
$cases = @(
    @{ Name = 'Device Detection'; Exe = 'PerformanceHashC'
        Arguments = @('--data-file', "$data/device-detection/TAC-HashV41.hash") }
    @{ Name = 'IP Intelligence'; Exe = 'PerformanceC'
        Arguments = @('--data-file', "$data/ip-intelligence/51Degrees-LiteV41.ipi",
            '--ip-addresses-file', "$data/ip-intelligence/evidence.yml") }
    @{ Name = 'Cache'; Exe = 'CachePerf'; Arguments = @() }
)
$bins = [ordered]@{}
foreach ($build in $Builds) {
    $name, $path = $build -split '=', 2
    $bins[$name] = $path
}
$baseName = @($bins.Keys)[0]

# Every rate in the example's JSON, keyed by the configuration it is under.
function Get-Rates($Json) {
    $rates = [ordered]@{}
    foreach ($key in $Json.Keys) {
        $value = $Json[$key]
        if ($value -is [System.Collections.IDictionary]) {
            foreach ($inner in $value.Keys) {
                if ($inner -like '*PerSecond') { $rates["$key $inner"] = [double]$value[$inner] }
            }
        } elseif ($key -like '*PerSecond' -and $key -notlike '*PerThread') {
            $rates[$key] = [double]$value
        }
    }
    $rates
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
$rows = [System.Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    for ($round = 1; $round -le $Rounds; $round++) {
        foreach ($name in $bins.Keys) {
            $json = Join-Path $OutDir 'last.json'
            Remove-Item $json -ErrorAction SilentlyContinue
            $path = Join-Path $bins[$name] "$($case.Exe)$exe"
            $arguments = $case.Exe -eq 'CachePerf' ? @($json) : (@('--json-output', $json) + $case.Arguments)
            Push-Location $RepoPath
            try {
                $text = (& $path @arguments) -join "`n"
                if ($LASTEXITCODE -ne 0) { Write-Host $text; throw "$($case.Exe) of $name exited with $LASTEXITCODE" }
            } finally {
                Pop-Location
            }
            $rates = Get-Rates (Get-Content $json -Raw | ConvertFrom-Json -AsHashtable)
            foreach ($metric in $rates.Keys) {
                $rows.Add([pscustomobject]@{ Case = $case.Name; Metric = $metric; Build = $name
                    Round = $round; Rate = $rates[$metric] })
            }
            Write-Host ("{0} {1} round {2}: {3}" -f $case.Name, $name, $round,
                (($rates.Keys | ForEach-Object { "$_ $('{0:N0}' -f $rates[$_])" }) -join ', '))
        }
    }
}
$os = $IsWindows ? 'Windows' : 'Linux'
$rows | Export-Csv -NoTypeInformation (Join-Path $OutDir "timing-$os.csv")

$headName = @($bins.Keys)[-1]
$lines = @("## $os, $baseName against $headName, middle of $Rounds rounds taking turns", '',
    "| Case | Rate | $baseName | Range | $headName | Range | Against $baseName | Rounds $headName won | Full ranges |",
    '| --- | --- | ---: | --- | ---: | --- | ---: | ---: | --- |')
foreach ($group in ($rows | Group-Object Case, Metric)) {
    $base = @($group.Group | Where-Object Build -eq $baseName | Sort-Object Round)
    $head = @($group.Group | Where-Object Build -eq $headName | Sort-Object Round)
    $bs = @($base.Rate | Sort-Object); $hs = @($head.Rate | Sort-Object)
    $bm = $bs[[int][Math]::Floor($bs.Count / 2)]; $hm = $hs[[int][Math]::Floor($hs.Count / 2)]
    $won = 0
    for ($i = 0; $i -lt [Math]::Min($base.Count, $head.Count); $i++) { if ($head[$i].Rate -gt $base[$i].Rate) { $won++ } }
    $apart = $hs[-1] -lt $bs[0] ? "$headName slower" : ($hs[0] -gt $bs[-1] ? "$headName faster" : 'overlap')
    $first = $group.Group[0]
    $lines += "| $($first.Case) | $($first.Metric) | $('{0:N0}' -f $bm) | $('{0:N0}' -f $bs[0]) to $('{0:N0}' -f $bs[-1]) | $('{0:N0}' -f $hm) | $('{0:N0}' -f $hs[0]) to $('{0:N0}' -f $hs[-1]) | $('{0:N2}' -f ($hm / $bm)) | $won of $($head.Count) | $apart |"
}
$lines | Set-Content (Join-Path $OutDir "timing-$os.md")
if ($env:GITHUB_STEP_SUMMARY) { $lines | Add-Content $env:GITHUB_STEP_SUMMARY }
$lines | Write-Host
