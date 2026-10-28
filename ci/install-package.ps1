param (
    [Parameter(Mandatory)][string]$RepoName
)

# The package is the build directory, with binaries and test artifacts.
Copy-Item -Recurse package/build $RepoName/

if (!$IsWindows) {
    # CTest needs to execute the test binaries.
    chmod -R 777 $RepoName/build
}
