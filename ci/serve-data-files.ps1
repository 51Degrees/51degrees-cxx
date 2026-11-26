<#
Serves the free data files fetch-assets has fetched from a local server, and
points every data file link in the data folder at that server.

An example that is missing its data file runs the data repository's script,
which downloads the file named in its .link file. With the links pointing
here, that download goes the way it does from a fresh clone and gets the
fetched file, and nothing is asked of the Distributor. A link whose file was
not fetched is answered 404, so the example fails naming the file.

Returns the server process, which the caller stops. The server writes one
line for each request to the log, ending with the path and the status, for
example "GET /device-detection/51Degrees-LiteV4.1.hash HTTP/1.1" 200.
#>
param (
    # The folder fetch-assets put the files in.
    [Parameter(Mandatory)][string]$Assets,
    # The data repository's folder, holding the .link files.
    [Parameter(Mandatory)][string]$Data,
    # An empty folder the server serves the files from.
    [Parameter(Mandatory)][string]$Folder,
    [Parameter(Mandatory)][string]$Log,
    [int]$Port = 8941
)
$ErrorActionPreference = 'Stop'

$ReadySeconds = 30
$RetryMilliseconds = 200

# The fetched files that are named differently in the data repository.
$dataNames = @{
    '51Degrees-LiteIpiV41.ipi' = '51Degrees-LiteV41.ipi'
}

$Data = (Resolve-Path -LiteralPath $Data).Path
$Folder = (New-Item -ItemType Directory -Force -Path $Folder).FullName
$fetched = @{}
foreach ($file in Get-ChildItem -LiteralPath $Assets -File) {
    $name = $dataNames.ContainsKey($file.Name) ? $dataNames[$file.Name] : $file.Name
    $fetched[$name] = $file.FullName
}

$links = @(Get-ChildItem -LiteralPath $Data -Recurse -File -Filter '*.link' |
    Where-Object { $_.FullName -notmatch '[\\/]\.git[\\/]' })
if ($links.Count -eq 0) {
    throw "$Data has no .link files"
}
foreach ($link in $links) {
    $name = $link.Name.Substring(0, $link.Name.Length - '.link'.Length)
    $path = [IO.Path]::GetRelativePath($Data, (Join-Path $link.DirectoryName $name))
    $segments = $path -split '[\\/]'
    if ($fetched.ContainsKey($name)) {
        $served = Join-Path $Folder $path
        New-Item -ItemType Directory -Force -Path (Split-Path $served) | Out-Null
        Copy-Item -LiteralPath $fetched[$name] -Destination $served
        Write-Host "Serving $($segments -join '/') from the fetched $name"
    }
    else {
        Write-Host "$($segments -join '/') was not fetched, so it is answered 404"
    }
    $url = "http://127.0.0.1:$Port/" + (($segments |
        ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/')
    Set-Content -LiteralPath $link.FullName -Value $url
}

# Windows has python, and the others python3.
$python = Get-Command ($IsWindows ? 'python' : 'python3') -CommandType Application `
    -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $python) {
    throw 'The data file server needs Python, which is not on the path'
}
$server = Start-Process -FilePath $python.Source -PassThru -NoNewWindow `
    -ArgumentList '-u', '-m', 'http.server', $Port, '--bind', '127.0.0.1',
        '--directory', "`"$Folder`"" `
    -RedirectStandardOutput "$Log.out" -RedirectStandardError $Log

# The server is ready once it takes a connection. A connection is not a
# request, so the log holds only the examples' requests.
$deadline = (Get-Date).AddSeconds($ReadySeconds)
while ($true) {
    $client = [Net.Sockets.TcpClient]::new()
    try {
        $client.Connect('127.0.0.1', $Port)
        break
    }
    catch {
        if ($server.HasExited -or (Get-Date) -gt $deadline) {
            if (-not $server.HasExited) { Stop-Process -Id $server.Id }
            Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue |
                Out-Host
            throw "The data file server did not start on port $Port"
        }
        Start-Sleep -Milliseconds $RetryMilliseconds
    }
    finally {
        $client.Dispose()
    }
}
Write-Host "The data file server is on port $Port"
$server
