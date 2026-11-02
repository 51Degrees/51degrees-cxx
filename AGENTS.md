# Working in this repository

Notes on what is not obvious from the code. Keep them short and current.
Building, testing and the layout are described in the [README](README.md).

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

## Balanced and required properties

Naming fewer properties than the data file has changes what the Balanced preset
holds in memory (`heldInit` in `src/ipi/ipi.c`). Anything not held reads from
the file, so a mistake in what is held makes a lookup slower and never wrong.
Memory must not rise above what naming nothing uses.

## Data files

The tests and examples find data by searching upward from their working
directory for a folder named `device-detection-data` or
`ip-intelligence-data`. Those names are fixed. The data repositories are
submodules at the root. The enterprise files used in CI are fetched by
`ci/fetch-assets.ps1` and are never committed.

## CI

The workflows in `.github/workflows` call the shared workflows in
[common-ci](https://github.com/51Degrees/common-ci), which run the scripts in
`ci/`. Each entry in `ci/options.json` lists the `Products` it builds and
tests. On a pull request, `ci/products.ps1` skips a product the change does
not touch. A change outside `src/dd`, `src/ipi` and their tests, examples,
docs and tools counts as touching every product.

`Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly` compiles both products with
`ExceptionsDisabled` on, which makes `fiftyoneDegreesException` a void
pointer. Its `BuildOnly` key leaves out the tests, which read the exception,
and the data files. So code outside the tests reads `exception->status` only
inside `#ifndef FIFTYONE_DEGREES_EXCEPTIONS_DISABLED`, or through the macros in
`src/common/exceptions.h`, which compile either way.

## Versions

The repository has one version, tagged as a plain version such as `4.5.96`.
`ci/get-next-package-version.ps1` moves only the patch number. Never move the
major or minor number, and never add a `+semver:` directive to a commit.

## History

The history of `common-cxx`, `device-detection-cxx`, `ip-intelligence-cxx`
and `ip-graph-cxx` was brought in with `git subtree add`, so commit SHAs match
the original repositories. Use `git log --follow` to trace a file back through
its move into this layout.
