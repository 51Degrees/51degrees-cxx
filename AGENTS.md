# Working in this repository

If you have been asked to get an example running, follow
[Run the examples](#run-the-examples). It gives every step in order, from a
fresh clone to the output. The sections after it are notes on what is not
obvious from the code, for changing it. Keep them short and current. The
[README](README.md) describes the build options, the tests and the layout.

## Run the examples

Run each command from the repository root unless the step says otherwise.
Device Detection's `GettingStartedHashC` is the quickest example to show,
because its data file is about 35 MB. IP Intelligence's `GettingStartedC`
needs a download of about 230 MB that unpacks to about 460 MB, so run it
second. This repository has no cloud examples, because the C and C++
libraries work only on-premise, with a data file.

### Prerequisites

- Git.
- CMake 3.24 or later.
- On Windows, Visual Studio 2019 or later, or the
  [C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/),
  with Platform Toolset `v142` and Windows SDK `10.0.18362.0` or later. CMake
  stops with an error that names whichever is too old.
- On Linux and macOS, a compiler that supports C11 and C++17. GCC must be 5.1
  or later. Linux builds link `libatomic`, which GCC 5 and later include.
- To download the data files, `curl`, `gzip` and `md5sum` (`md5` on macOS)
  for the shell script, or Windows PowerShell 5.1 or PowerShell 7 for the
  PowerShell script.

Clone the repository with its submodules.

```
git clone --recurse-submodules https://github.com/51Degrees/51degrees-cxx.git
cd 51degrees-cxx
```

In a clone made without `--recurse-submodules`, fetch the submodules.

```
git submodule update --init
```

### Data files

The examples use the free Lite data files, which need no licence key. They are
not stored in this repository. The `data` submodule, which is the
[data](https://github.com/51Degrees/data) repository, holds a link to each
file, and a script downloads them into `data/device-detection` and
`data/ip-intelligence`.

```
sh data/get-data-files.sh
```

In PowerShell, run `./data/get-data-files.ps1` instead. Either script
downloads every free file, and keeps a file that is already there.

#### Check the files

- `data/device-detection/51Degrees-LiteV4.1.hash` is more than 10 MB.
- `data/ip-intelligence/51Degrees-LiteV41.ipi` is more than 100 MB.

### Build

Build in a folder named `build`. `-DBUILD_TESTING=OFF` leaves out the tests
and the Google Test download, so the build is quicker, and every example is
still built. To build one product only, also add
`-DFIFTYONE_BUILD_IP_INTELLIGENCE=OFF` or `-DFIFTYONE_BUILD_DEVICE_DETECTION=OFF`
to the first `cmake` command.

On Linux and macOS, build with these commands.

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build . --config Release
cd ..
```

On Windows, add `-A x64` for a 64 bit build.

```
mkdir build
cd build
cmake .. -A x64 -DBUILD_TESTING=OFF
cmake --build . --config Release
cd ..
```

The examples are built into `build/bin`. On Windows they are in
`build\bin\Release`, because a Visual Studio build puts each configuration in
a folder of its own.

| Language | Device Detection | IP Intelligence |
| --- | --- | --- |
| C | `GettingStartedHashC` | `GettingStartedC` |
| C++ | `GettingStartedHashCPP` | `GettingStartedCPP` |

### Run

An example finds its data file by searching the folder it runs in and each
folder above it, so run it from the repository root. The getting-started
examples, like most of the others, wait for a key press before they exit, so
give them empty input or they will look as if they have hung.

```
./build/bin/GettingStartedHashC < /dev/null
./build/bin/GettingStartedC < /dev/null
```

On Windows, in `cmd.exe`, run these.

```
build\bin\Release\GettingStartedHashC.exe < NUL
build\bin\Release\GettingStartedC.exe < NUL
```

In PowerShell, pipe in an empty line instead, for example
`'' | .\build\bin\Release\GettingStartedHashC.exe`.

To use another data file, give its path as the first argument. The IP
Intelligence examples also read a path from the `51DEGREES_IPI_PATH`
environment variable. The name starts with a digit, so in a Linux shell set it
with `env`, for example
`env 51DEGREES_IPI_PATH=/path/to/file.ipi ./build/bin/GettingStartedC < /dev/null`.

### Correct output

`GettingStartedHashC` processes six sets of evidence. For each set it prints
`Input values:` and the headers, then `Results:` with `Mobile Device`,
`Platform Name`, `Platform Version`, `Browser Name`, `Browser Version`,
`Device ID`, `JSON` and `GetHighEntropyValues JS`, and it ends the set with
`---###---`. After the last set it names the data file, in lines like these.

```
Using a Lite data file created <date> from location <path>
This example is using the "Lite" data file. This is used for illustration, ...
```

It may also print, in yellow, a warning that the data file is more than 30
days old. The Lite file is updated less often than the paid files, so the
warning is expected. The exit code is 0.

`GettingStartedC` prints `Starting Getting Started Example.`, then looks up
the IPv4 address `185.28.167.77` and then an IPv6 address. For each address it
prints a line for each property, such as `- RegisteredCountry: ...`, and then,
under `(Results using ResultsIpiGetValuesCollection):`, each value with its
type and weight, as `- [<property>] (<type>) <x<weight>> <value>`. The Lite
file has RegisteredCountry, RegisteredName and RegisteredOwner, so the lines
for the properties it does not have, such as `Longitude`, are empty. The exit
code is 0.

The C++ examples print the same kind of values in a different layout. The paid
data files have many more properties, as the
[pricing page](https://51degrees.com/pricing?utm_source=github&utm_medium=docs&utm_campaign=51degrees-cxx&utm_content=agents.md&utm_term=correct-output)
describes.

### Pitfalls

- **A data file that is not there.** `data` holds a `.link` for each file
  rather than the file. An example whose file is missing runs the download
  script for it, so it needs the network and the tools under "Prerequisites".
  Check the file sizes as above.
- **Missing submodules.** Without them the example cannot find a data file and
  says so. A zip download of the repository has no submodules, so clone with
  Git.
- **An example that seems to hang.** It is waiting for a key press. Give it
  empty input, as "Run" shows.
- **Windows line endings.** Git for Windows usually checks files out with
  `core.autocrlf=true`. `data` keeps LF line endings for its shell script and
  its links, so both work from a Windows checkout, but its IP Intelligence
  evidence files get Windows line endings. So do not build and test in WSL
  from a checkout made on Windows, because the Linux tests then read IP
  addresses ending in a carriage return and fail. Clone again inside WSL.
- **Warnings are errors.** `CMakeLists.txt` sets
  `CMAKE_COMPILE_WARNING_AS_ERROR`, and the Device Detection examples build
  with `-Werror` or `/WX`, so a compiler newer than the one CI uses can stop the
  build on a new warning.
- **Memory.** `GettingStartedC` loads the whole IP Intelligence data file into
  memory (`IpiInMemoryConfig`), so it needs about 0.5 GB free.
- **What CI does not test.** CI builds IP Intelligence on Windows and Linux,
  not macOS (`ci/options.json`). It never runs the example programs, only the
  tests that call each example's code. For IP Intelligence it links a paid data
  file under the Lite file's name (`ci/fetch-assets.ps1`), so the Lite IP
  Intelligence file itself is never used in CI.

## The two copies of common

Device Detection and IP Intelligence need the common library built
differently. IP Intelligence data files can exceed 4 GB and use a reduced
profile record, so its copy is compiled with
`FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT` and `FIFTYONE_DEGREES_REDUCED_FILE`.
Both change structure sizes, so the copies cannot be shared.

- CMake builds `fiftyone-common-c` and `fiftyone-common-cxx` for Device
  Detection, and `fiftyone-common-c-ipi` and `fiftyone-common-cxx-ipi` for IP
  Intelligence (`src/common/CMakeLists.txt`). The definitions are public on
  the `-ipi` targets, so never add them globally.
- Visual Studio builds common for IP Intelligence when the solution is
  `IpIntelligence`, into its own `Intermediate\Ipi` folder
  (`VisualStudio/Common/Library.Build.props`).
- The common tests run against both copies, as `CommonTests` and
  `CommonTestsIpi`.

Both copies would export the same names, so `src/common/ipiSymbols.h` lists
every common function and global and renames them with an `ipi_` prefix. Common
headers include it first when `FIFTYONE_DEGREES_IPI_SYMBOLS` is defined.

That define must never change anything for a language wrapper. CMake sets it on
the IP Intelligence copy through the `FIFTYONE_IPI_SYMBOLS` option, which is on
only when this repository is the top level project. A project that includes this
CMake file, and any build that compiles the sources itself, keeps the original
names and doesn't need the list. `Ubuntu_arm64_Release` in `ci/options.json`
builds with the option off, so CI keeps testing those names.

When you add a common function or global, add it to the list. A new common
header includes the list first, in the same way as the others.
`ci/check-common-symbols.ps1` fails the build otherwise, and `test/both` links
both products into one program.

The common C++ classes have the same problem, so each common C++ header opens
`FIFTYONE_DEGREES_COMMON_CXX_BEGIN` inside its namespace. That expands to nothing
unless `FIFTYONE_DEGREES_IPI_CXX_SYMBOLS` is defined, which makes it an inline
namespace, changing linked names and not source names. The CMake option
`FIFTYONE_CXX_BOTH_PRODUCTS` turns it on, and it stays off by default until the
wrappers SWIG generates have been checked. The
`Ubuntu_x64_Release_CxxBothProducts` entry in `ci/options.json` builds and tests
with it on.

## Includes are relative

Sources include each other by relative path, for example `src/dd/hash/hash.h`
includes `../../common/common.h`. The APIs for other languages compile these
sources with their own build systems, so keep includes relative to the file
rather than relying on include directories.

## HighPerformance and required properties

Naming fewer properties than the data file has changes what the HighPerformance
preset loads into memory (`heldInit` in `src/ipi/ipi.c`). Anything not loaded
reads from the file, so a mistake in what is loaded makes a lookup slower and
never wrong. A lookup that reads the named properties must read nothing from
the file. Balanced holds the same whatever is named.

## Data files

The tests and examples find data by searching upward from their working
directory for the folder `data/device-detection` or `data/ip-intelligence`.
Those names are fixed. `data` is the data repository, a submodule at the root,
which holds a `<file>.link` for each free data file rather than the file.
`data/get-data-files.ps1` and `data/get-data-files.sh` download them, and an
example whose file is missing runs the script for it. The
`device-detection-data` and `ip-intelligence-data` submodules are kept only
until the other language APIs read `data`, so nothing here uses them. The
enterprise files used in CI are fetched by `ci/fetch-assets.ps1` and are never
committed.

## CI

The workflows in `.github/workflows` call the shared workflows in
[common-ci](https://github.com/51Degrees/common-ci), which run the scripts in
`ci/`. Each entry in `ci/options.json` lists the `Products` it builds and
tests. On a pull request, `ci/products.ps1` skips a product the change does
not touch. A change outside `src/dd`, `src/ipi` and their tests, examples,
docs and tools counts as touching every product.

`ci/select-options.ps1` chooses the entries a pull request runs. The standard
set builds the products a change touches with GCC and MSVC, without the IP
Intelligence tests unless an IP Intelligence file changed in more than its
comments. A change to a product's source adds that product's other entries.
The repeated Debug builds and macOS run only when `rc` itself or the release
pull request is built, which `.github/workflows/release-pull-request.yml`
does on every entry.

Pull requests target `rc`, not `main`, because every language takes
each commit on `main` at its next nightly. The README's "Making changes" says
how a change reaches `main`.

`Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly` compiles both products with
`ExceptionsDisabled` on, which makes `fiftyoneDegreesException` a void
pointer. Its `BuildOnly` key leaves out the tests, which read the exception,
and the data files. So code outside the tests reads `exception->status` only
inside `#ifndef FIFTYONE_DEGREES_EXCEPTIONS_DISABLED`, or through the macros in
`src/common/exceptions.h`, which compile either way.

CMake builds every test file in a test folder, but the Visual Studio test
projects list theirs by name, so add a new test file to its `.vcxproj` and
`.vcxproj.filters`. `ci/check-visual-studio-tests.ps1` fails the Visual Studio
builds when one is missing. Those builds run the common and Device Detection
tests, and build the IP Intelligence tests without running them.

## Releases

Pull requests go to `rc`, not `main`, and stay drafts until they have been
reviewed, because the nightly merges any pull request into `rc` that is not a
draft and has no review requested. When a release is wanted, one pull request
merges `rc` into `main`, and the "Only rc into main" check fails a pull request
into `main` from any other branch.
The steps are in the README's [Making changes](README.md#making-changes)
section.

## Versions

The repository has one version, tagged as a plain version such as `4.5.96`.
`ci/get-next-package-version.ps1` moves only the patch number. Never move the
major or minor number, and never add a `+semver:` directive to a commit.

## History

The history of `common-cxx`, `device-detection-cxx`, `ip-intelligence-cxx`
and `ip-graph-cxx` was brought in with `git subtree add`, so commit SHAs match
the original repositories. Use `git log --follow` to trace a file back through
its move into this layout.
