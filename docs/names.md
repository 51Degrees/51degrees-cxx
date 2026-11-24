# Names this repository reads

Every environment variable, secret and build option that 51Degrees defines
starts `_51DEGREES_`, so that wherever one appears it is clearly a 51Degrees
name. This page lists every name the code, examples, tests, build files and
workflows of this repository read or define. The test in
[`test/ipi/EnvironmentVariableNameTests.cpp`](../test/ipi/EnvironmentVariableNameTests.cpp)
reads the tables below and fails when a tracked file reads a name that is not
listed, or when a 51Degrees name does not start `_51DEGREES_`. It also fails
when a job that calls a shared workflow of common-ci passes a secret by name.
Add a name here in the same change that first reads it.

## Environment variables

| Name | Read by | What it holds |
| --- | --- | --- |
| `_51DEGREES_DD_PATH` | The Device Detection examples, in `examples/dd/C/Hash/ExampleBase.c`, and set by `ci/fetch-assets.ps1` | The path of the Device Detection data file to use in place of the one in `data/device-detection` |
| `_51DEGREES_IPI_PATH` | The IP Intelligence examples, in `examples/ipi/Base/ExampleBase.c`, and set by `ci/fetch-assets.ps1` | The path of the IP Intelligence data file to use in place of the one in `data/ip-intelligence` |

A data file path given as a Device Detection example's first argument comes
before `_51DEGREES_DD_PATH`.

## Secrets

A job that calls a shared workflow of
[common-ci](https://github.com/51Degrees/common-ci) passes `secrets: inherit`
and names no secret. The shared workflow puts each secret in the environment
of the job under its own name, and the script that uses it reads it there. No
workflow of this repository reads a secret by name.

| Name | Read by | What it holds |
| --- | --- | --- |
| `_51DEGREES_ACCESS_TOKEN` | The shared workflows of common-ci | The token the shared workflows use to read and write this repository and its pull requests |
| `_51DEGREES_LICENSE_KEY` | The data file download of common-ci, which `ci/fetch-assets.ps1` calls | The license key the licensed data files are downloaded with |

## CMake options

| Name | Default | What it does |
| --- | --- | --- |
| `_51DEGREES_BUILD_DEVICE_DETECTION` | `ON` | Builds the Device Detection libraries, examples and tests |
| `_51DEGREES_BUILD_IP_INTELLIGENCE` | `ON` | Builds the IP Intelligence libraries, examples and tests |
| `_51DEGREES_BUILD_EXAMPLES` | `ON` | Builds the examples and the `js-snippet-export` tool. Always on when `BUILD_TESTING` is on |
| `_51DEGREES_COMMON_CXX_BUILD_TESTING` | `ON` | Builds the common library's own tests, when `BUILD_TESTING` is on |
| `_51DEGREES_COMMON_CXX_BUILD_TESTING_IPI` | `ON` on 64 bit | Also tests the copy of common that IP Intelligence uses |
| `_51DEGREES_IPI_SYMBOLS` | `ON` when this repository is the top level build | Gives IP Intelligence's copy of common its own linked names |
| `_51DEGREES_CXX_BOTH_PRODUCTS` | `OFF` | Gives IP Intelligence's copy of the common C++ classes its own linked names |
| `_51DEGREES_INSTALL` | `ON` when this repository is the top level build | Adds the install rules |
| `_51DEGREES_VERSION` | The nearest version tag in a Git clone, otherwise `0.0.0` | The version the installed package reports |
| `_51DEGREES_32BIT` | `OFF` | Builds 32 bit libraries and programs |
| `_51DEGREES_MEMORY_ONLY` | `OFF` | Defines `FIFTYONE_DEGREES_MEMORY_ONLY` |
| `_51DEGREES_NO_THREADING` | `OFF` | Defines `FIFTYONE_DEGREES_NO_THREADING` for C |
| `_51DEGREES_EXCEPTIONS_DISABLED` | `OFF` | Defines `FIFTYONE_DEGREES_EXCEPTIONS_DISABLED` |
| `_51DEGREES_LARGE_DATA_FILE_SUPPORT` | `OFF` | Defines `FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT` for the copy of common that Device Detection uses |
| `_51DEGREES_REDUCED_FILE` | `OFF` | Defines `FIFTYONE_DEGREES_REDUCED_FILE` for the copy of common that Device Detection uses |
| `_51DEGREES_WITH_ASAN` | `OFF` | Builds every target with AddressSanitizer |
| `_51DEGREES_EXPECTED_VERSION` | Empty | In `test/install`, the version the installed package must report |
| `_51DEGREES_EXAMPLES` | Empty | In `test/install/examples`, the copy of the examples folder to build against the install |

The C preprocessor macros of the libraries, which start `FIFTYONE_DEGREES_`,
are identifiers in the code and are described in the
[common library README](../src/common/README.md#build-options).

## Names other tools define

These names belong to the tool that defines them, and are used as that tool
requires.

| Name | Defined by | Read by |
| --- | --- | --- |
| `CI` | GitHub Actions | The IP Intelligence tests, which skip the temporary file tests there |
| `GITHUB_ACTIONS` | GitHub Actions | The examples, which leave the source out of a data file download there |
| `GITHUB_ENV` | GitHub Actions | `ci/fetch-assets.ps1`, which adds the data file paths to it for the steps after it |
| `HOME` | The operating system | `cmake/FindGccAtomic.cmake`, to find a locally installed `libatomic` |
| `LC_ALL` | The operating system | Set, not read, by the single file build workflow |
| `ProgramFiles(x86)` | The operating system | `ci/cxx/run-unit-tests.ps1` and `ci/check-common-symbols.ps1`, to find the debugger and Visual Studio on Windows |
| `RUNNER_TEMP` | GitHub Actions | `ci/cxx/run-unit-tests.ps1`, the folder a crash dump is written to and deleted from |
| `gtest_force_shared_crt` | GoogleTest | The GoogleTest build, set in `CMakeLists.txt` |
