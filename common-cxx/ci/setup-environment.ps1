param(
    [string]$ProjectDir = ".",
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

    # Shared, because the same call in five repositories carried the same
    # fault: these packages do not exist on arm64, so asking for them on
    # an ARM runner fails the step. See common-ci environments/README.md.
    ./environments/setup-multilib.ps1 -Packages gcc-multilib

}

if ($IsLinux -or $IsMacOS) {
    pipx install 'gcovr~=7.2' || $(throw "gcovr install failed")
}
