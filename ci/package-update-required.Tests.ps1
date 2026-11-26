# Each test makes a repository with one release tag, commits a change to the
# files it names and runs the script, which exits with 0 when a release is
# needed and 1 when it is not.
BeforeAll {
    $script:Script = Join-Path $PSScriptRoot 'package-update-required.ps1'

    function New-Repository {
        $path = Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid())
        New-Item -ItemType Directory $path | Out-Null
        git -C $path init --quiet
        git -C $path config user.name 'Test'
        git -C $path config user.email 'test@example.com'
        git -C $path config commit.gpgsign false
        Set-Content (Join-Path $path 'CMakeLists.txt') 'first'
        git -C $path add -A
        git -C $path commit --quiet -m 'First'
        return $path
    }

    function Add-Change([string]$Path, [string[]]$Files) {
        foreach ($file in $Files) {
            $full = Join-Path $Path $file
            New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
            Add-Content $full 'changed'
        }
        git -C $Path add -A
        git -C $Path commit --quiet -m 'Change'
    }

    function Get-ExitCode([string]$Path, [string]$Version) {
        pwsh -NoProfile -File $script:Script -RepoName $Path `
            -Version $Version | Out-Null
        return $LASTEXITCODE
    }
}

Describe 'package-update-required' {
    BeforeEach {
        $script:Repo = New-Repository
        git -C $script:Repo tag 4.5.1
    }

    AfterEach {
        Remove-Item -Recurse -Force $script:Repo
    }

    It 'needs no release when only <Files> changed' -ForEach @(
        @{ Files = @('ci/build-project.ps1', 'ci/options.json') }
        @{ Files = @('.gitmodules') }
        @{ Files = @(
            'VisualStudio/Common/Fiftyone.Common.Tests/Fiftyone.Common.Tests.vcxproj') }
        @{ Files = @('README.md', 'src/common/README.md') }
        @{ Files = @('.github/workflows/examples.yml') }
        @{ Files = @(
            'examples/dd/C/Hash/GettingStarted.c',
            'VisualStudio/DeviceDetection/Examples/C/Hash/Getting Started/Getting Started.vcxproj') }
        @{ Files = @('test/common/CacheTests.cpp', 'src/common/tests/Tests.cpp') }
        @{ Files = @('LICENSE', 'docs/path-map.md') }
        @{ Files = @('data/device-detection/file.link') }
    ) {
        Add-Change $script:Repo $Files
        Get-ExitCode $script:Repo '4.5.2' | Should -Be 1
    }

    It 'needs a release when <Files> changed' -ForEach @(
        @{ Files = @('src/common/cache.c') }
        @{ Files = @('CMakeLists.txt') }
        @{ Files = @('cmake/install.cmake') }
        @{ Files = @(
            'VisualStudio/Common/FiftyOne.Common.C/FiftyOne.Common.C.vcxproj') }
        @{ Files = @('ci/build-project.ps1', 'src/common/cache.c') }
        @{ Files = @('src/contests/file.c') }
        @{ Files = @('src/ci/file.c') }
    ) {
        Add-Change $script:Repo $Files
        Get-ExitCode $script:Repo '4.5.2' | Should -Be 0
    }

    It 'needs no release when the version is already tagged' {
        Add-Change $script:Repo @('src/common/cache.c')
        Get-ExitCode $script:Repo '4.5.1' | Should -Be 1
    }

    It 'needs a release when there is no tag to compare with' {
        git -C $script:Repo tag -d 4.5.1 | Out-Null
        Add-Change $script:Repo @('ci/build-project.ps1')
        Get-ExitCode $script:Repo '4.5.2' | Should -Be 0
    }
}
