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

## Includes are relative

Sources include each other by relative path, for example `src/dd/hash/hash.h`
includes `../../common/common.h`. The APIs for other languages compile these
sources with their own build systems, so keep includes relative to the file
rather than relying on include directories.

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

## Versions

The repository has one version, tagged as a plain version such as `4.5.96`.
`ci/get-next-package-version.ps1` moves only the patch number. Never move the
major or minor number, and never add a `+semver:` directive to a commit.

## History

The history of `common-cxx`, `device-detection-cxx`, `ip-intelligence-cxx`
and `ip-graph-cxx` was brought in with `git subtree add`, so commit SHAs match
the original repositories. Use `git log --follow` to trace a file back through
its move into this layout.
