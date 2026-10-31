param(
    [string]$Name = "Release_x64",
    [string]$Arch = "x64",
    [string]$Configuration = "Release",
    [string]$BuildMethod = "cmake"
)

if ($BuildMethod -eq "msbuild") {

    # Setup the MSBuild environment if it is required.
    ./environments/setup-msbuild.ps1

}

if ($IsLinux) {

    # The x86 jobs build 32 bit C and C++, which needs both multilib packages.
    # The shared script skips them on arm64, where they do not exist.
    ./environments/setup-multilib.ps1

}

if ($IsLinux -or $IsMacOS) {
    pipx install 'gcovr~=7.2' || $(throw "gcovr install failed")
}
