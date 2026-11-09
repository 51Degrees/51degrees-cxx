<#
.SYNOPSIS
Checks that the libraries install and that another CMake project can use them.

.DESCRIPTION
Builds the libraries from an archive of HEAD with the tests and examples off,
installs them, builds test/install against the install with find_package, and
runs each of its programs. The archive holds what GitHub's source archive
holds, without the submodules, so the build shows it needs no data files. On
Linux the build also runs with no network. Each program runs once with no data
file, and again with the product's data file when the job has fetched it.

.PARAMETER Options
The CMake options of the job's own build, which the install build repeats.
#>
param(
    [Parameter(Mandatory)][string]$RepoName,
    [string]$Arch = 'x64',
    [string]$Configuration = 'Release',
    [string[]]$Products,
    [string[]]$Options
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$installed = @($Products | Where-Object { $_ -in 'dd', 'ipi' })
if (!$installed) {
    Write-Output 'No product is built, so nothing is installed'
    exit 0
}

$RepoPath = (Resolve-Path $RepoName).Path
$Work = "$RepoPath/build-install"
Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
$Source = (New-Item -ItemType Directory -Force "$Work/source").FullName
$Prefix = "$Work/prefix"

git -C $RepoPath archive --format=zip --output "$Work/source.zip" HEAD
Expand-Archive "$Work/source.zip" -DestinationPath $Source

# An archive has no history, so it is given the version, as a package
# manager does.
$version = & {
    $PSNativeCommandUseErrorActionPreference = $false
    git -C $RepoPath describe --tags --abbrev=0 --match '[0-9]*.[0-9]*.[0-9]*' 2>$null
}
$version = $version ? $version : '0.0.0'
$global:LASTEXITCODE = 0
Write-Output "Installing version $version of $($installed -join ', ')"

$platform = @()
if ($IsWindows) {
    $platform = @('-A', ($Arch -eq 'x86' ? 'Win32' : 'x64'))
}

# Ubuntu lets only root make a namespace with no network unless this is off.
if ($IsLinux -and (Test-Path /proc/sys/kernel/apparmor_restrict_unprivileged_userns)) {
    sudo sysctl -q -w kernel.apparmor_restrict_unprivileged_userns=0
}
if ($IsLinux) {
    & {
        $PSNativeCommandUseErrorActionPreference = $false
        unshare --map-root-user --net curl -sS -m 10 -o /dev/null https://github.com 2>$null
    }
    if ($LASTEXITCODE -eq 0) { throw 'A download worked with no network' }
    $global:LASTEXITCODE = 0
    Write-Output 'The build runs where a download fails, as it has no network'
}
function Invoke-Offline([string[]]$Arguments) {
    if ($IsLinux) {
        unshare --map-root-user --net cmake @Arguments
    } else {
        cmake @Arguments
    }
}

Invoke-Offline (@('-S', $Source, '-B', "$Work/build") + $Options + $platform + @(
    "-DCMAKE_BUILD_TYPE=$Configuration"
    "-DCMAKE_INSTALL_PREFIX=$Prefix"
    '-DBUILD_TESTING=OFF'
    '-DFIFTYONE_BUILD_EXAMPLES=OFF'
    "-DFIFTYONE_VERSION=$version"))
Invoke-Offline @('--build', "$Work/build", '--config', $Configuration,
    '--parallel', [Environment]::ProcessorCount)
cmake --install "$Work/build" --config $Configuration

Write-Output 'Installed files:'
Get-ChildItem -Recurse -File $Prefix |
    ForEach-Object { [IO.Path]::GetRelativePath($Prefix, $_.FullName) } |
    Sort-Object

cmake -S "$Source/test/install" -B "$Work/consumer" @platform `
    "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$Prefix" `
    "-DEXPECTED_VERSION=$version"
cmake --build "$Work/consumer" --config $Configuration

$bin = $IsWindows ? "$Work/consumer/$Configuration" : "$Work/consumer"
$exe = $IsWindows ? '.exe' : ''
$data = @{
    dd = "$RepoPath/data/device-detection/51Degrees-LiteV4.1.hash"
    ipi = "$RepoPath/data/ip-intelligence/51Degrees-LiteV41.ipi"
}
foreach ($product in $installed) {
    foreach ($language in 'c', 'cxx') {
        $program = "$bin/$product-$language$exe"
        Write-Output "Running $product-$language with no data file"
        & $program
        if (Test-Path $data[$product]) {
            Write-Output "Running $product-$language with $($data[$product])"
            & $program $data[$product]
        }
    }
}
