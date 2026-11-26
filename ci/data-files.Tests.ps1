# Checks that no CI job downloads a data file itself. The jobs take the free
# data files from common-ci's fetch-assets step, which fetches them or
# restores them from the asset cache, and the examples, whose own download is
# what a fresh clone tests, get them from a local server holding those files.
BeforeAll {
    $script:Workflows = Join-Path $PSScriptRoot '../.github/workflows'
    $script:FetchAssets = 'uses: 51Degrees/common-ci/actions/fetch-assets@'

    # The lines of a workflow that are not comments.
    function Get-Code([string]$Name) {
        @(Get-Content (Join-Path $script:Workflows $Name) |
            Where-Object { $_ -notmatch '^\s*#' })
    }

    # Each fetch-assets step of a workflow, with the line it starts on, its
    # assets, and its cache and lazy inputs.
    function Get-FetchSteps([string]$Name) {
        $lines = Get-Code $Name
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -notmatch [regex]::Escape($script:FetchAssets)) {
                continue
            }
            $step = [ordered]@{ Line = $i; Assets = @(); Cache = $null; Lazy = $null }
            $indent = $lines[$i].Length - $lines[$i].TrimStart().Length
            for ($j = $i + 1; $j -lt $lines.Count; $j++) {
                $text = $lines[$j]
                $at = $text.Length - $text.TrimStart().Length
                if ($text.Trim() -and $at -lt $indent) { break }
                if ($text -match '^\s+assets: \|') {
                    $list = $at
                    for ($j++; $j -lt $lines.Count; $j++) {
                        $item = $lines[$j]
                        if ($item.Length - $item.TrimStart().Length -le $list) {
                            $j--
                            break
                        }
                        $step.Assets += $item.Trim()
                    }
                }
                elseif ($text -match "^\s+cache: '?(\w+)'?") { $step.Cache = $Matches[1] }
                elseif ($text -match "^\s+lazy: '?(\w+)'?") { $step.Lazy = $Matches[1] }
            }
            [pscustomobject]$step
        }
    }

    function Get-LineOf([string]$Name, [string]$Pattern) {
        $lines = Get-Code $Name
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match $Pattern) { return $i }
        }
        return -1
    }

    $script:FreeFiles = @('51Degrees-LiteV4.1.hash', '20000 Evidence Records.yml',
        '51Degrees-LiteIpiV41.ipi', '51Degrees-IPIV4AsnIpiV41.ipi')
}

Describe 'The CI workflows' {
    It 'run no download of a data file in <_>' -ForEach @(
        Get-ChildItem (Join-Path $PSScriptRoot '../.github/workflows') -Filter '*.yml' |
            ForEach-Object Name) {
        $download = 'get-data-files|distributor\.51degrees\.com|\bcurl\b|' +
            '\bwget\b|Invoke-WebRequest|Invoke-RestMethod'
        @(Get-Code $_ | Where-Object { $_ -match $download }) | Should -BeNullOrEmpty
    }
}

Describe 'examples.yml' {
    BeforeAll {
        $script:Steps = @(Get-FetchSteps 'examples.yml')
    }

    It 'fetches the free files once, then restores them in each examples job' {
        $Steps.Count | Should -Be 2
        $Steps[0].Lazy | Should -Be 'true'
        $Steps[1].Lazy | Should -BeNullOrEmpty
        foreach ($step in $Steps) {
            $step.Cache | Should -Be 'true'
            $step.Assets -join '|' | Should -Be ($FreeFiles -join '|')
        }
        (Get-LineOf 'examples.yml' '^\s+needs: fetch$') | Should -BeGreaterThan $Steps[0].Line
        (Get-LineOf 'examples.yml' '^\s+needs: fetch$') | Should -BeLessThan $Steps[1].Line
    }

    It 'points the data file links at the local server before an example runs' {
        $serve = Get-LineOf 'examples.yml' '\./ci/serve-data-files\.ps1 -Assets assets -Data data'
        $run = Get-LineOf 'examples.yml' 'Start-Process -FilePath \(Resolve-Path \$exe\)'
        $serve | Should -BeGreaterThan $Steps[1].Line
        $serve | Should -BeLessThan $run
    }
}

Describe 'thread-sanitizer.yml' {
    It 'fetches the Device Detection files before the tests are built' {
        $steps = @(Get-FetchSteps 'thread-sanitizer.yml')
        $steps.Count | Should -Be 1
        $steps[0].Cache | Should -Be 'true'
        $steps[0].Assets -join '|' | Should -Be ($FreeFiles[0..1] -join '|')
        $build = Get-LineOf 'thread-sanitizer.yml' 'name: Build the tests'
        $steps[0].Line | Should -BeLessThan $build
        $copy = Get-LineOf 'thread-sanitizer.yml' 'Copy-Item .*assets/51Degrees-LiteV4\.1\.hash'
        $copy | Should -BeGreaterThan $steps[0].Line
        $copy | Should -BeLessThan $build
    }
}

Describe 'serve-data-files' {
    BeforeAll {
        $script:Port = 8942
        $data = New-Item -ItemType Directory "$TestDrive/data"
        New-Item -ItemType Directory "$data/device-detection", "$data/ip-intelligence" |
            Out-Null
        $real = 'https://distributor.51degrees.com/api/v2/free/'
        Set-Content "$data/device-detection/51Degrees-LiteV4.1.hash.link" "${real}lite-hash-v41"
        Set-Content "$data/device-detection/20000 Evidence Records.yml.link" "${real}evidence"
        Set-Content "$data/device-detection/20000 User Agents.csv.link" 'https://example.com/ua.csv'
        Set-Content "$data/ip-intelligence/51Degrees-LiteV41.ipi.link" "${real}ipi-lite-v41"
        $assets = New-Item -ItemType Directory "$TestDrive/assets"
        Set-Content "$assets/51Degrees-LiteV4.1.hash" 'hash' -NoNewline
        Set-Content "$assets/20000 Evidence Records.yml" 'evidence' -NoNewline
        Set-Content "$assets/51Degrees-LiteIpiV41.ipi" 'ipi' -NoNewline
        $script:Log = "$TestDrive/server.log"
        $script:Server = & "$PSScriptRoot/serve-data-files.ps1" -Assets $assets `
            -Data $data -Folder "$TestDrive/served" -Log $Log -Port $Port 6>$null
        $script:Links = @(Get-ChildItem $data -Recurse -Filter '*.link')

        function Get-Linked([string]$Link) {
            $url = (Get-Content -LiteralPath $Link -TotalCount 1).Trim()
            Invoke-WebRequest -Uri $url -SkipHttpErrorCheck
        }
    }

    AfterAll {
        if ($Server -and -not $Server.HasExited) { Stop-Process -Id $Server.Id }
    }

    It 'points every link at the local server and none at the Distributor' {
        $Links.Count | Should -Be 4
        foreach ($link in $Links) {
            Get-Content -LiteralPath $link.FullName -Raw |
                Should -Match "^http://127\.0\.0\.1:$Port/"
        }
    }

    It 'serves each fetched file under its data repository name' {
        $answers = @{
            'device-detection/51Degrees-LiteV4.1.hash.link' = 'hash'
            'device-detection/20000 Evidence Records.yml.link' = 'evidence'
            'ip-intelligence/51Degrees-LiteV41.ipi.link' = 'ipi'
        }
        foreach ($link in $answers.Keys) {
            $response = Get-Linked "$TestDrive/data/$link"
            $response.StatusCode | Should -Be 200 -Because $link
            [Text.Encoding]::UTF8.GetString($response.RawContentStream.ToArray()) |
                Should -Be $answers[$link] -Because $link
        }
    }

    It 'answers 404 for a file that was not fetched' {
        (Get-Linked "$TestDrive/data/device-detection/20000 User Agents.csv.link").StatusCode |
            Should -Be 404
    }

    It 'logs each request with its path and status' {
        $requests = @(Get-Content $Log | Where-Object { $_ -match '"GET /' })
        $requests | Should -HaveCount 4
        $requests -match '"GET /device-detection/20000%20Evidence%20Records\.yml HTTP/1\.1" 200' |
            Should -HaveCount 1
        $requests -match '"GET /device-detection/20000%20User%20Agents\.csv HTTP/1\.1" 404' |
            Should -HaveCount 1
    }
}
