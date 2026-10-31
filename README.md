# 51Degrees C and C++

![51Degrees](https://51degrees.com/img/logo.png?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-c-and-cpp "Data rewards the curious") **C and C++ libraries for Device Detection and IP Intelligence**

This repository holds the 51Degrees C and C++ code: the common library, Device
Detection and IP Intelligence. The 51Degrees APIs for other languages build on
these sources.

| Product | Read first | Documentation |
| --- | --- | --- |
| Device Detection | [docs/dd/README.md](docs/dd/README.md) | [C](https://51degrees.com/documentation/apis/device-detection-cxx/index.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=device-detection-c) and [C++](https://51degrees.com/documentation/apis/device-detection-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=device-detection-cpp) |
| IP Intelligence | [docs/ipi/README.md](docs/ipi/README.md) | [C](https://51degrees.com/documentation/apis/ip-intelligence-cxx/index.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=ip-intelligence-c) and [C++](https://51degrees.com/documentation/apis/ip-intelligence-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=ip-intelligence-cpp) |
| Common library | [src/common/README.md](src/common/README.md) | |

## What is where

| Path | Contents |
| --- | --- |
| `src/common` | The common library both products are built on |
| `src/dd` | Device Detection, with the Hash algorithm in `src/dd/hash` |
| `src/ipi` | IP Intelligence, with the IP graph in `src/ipi/graph` |
| `test/common`, `test/dd`, `test/ipi` | Unit, integration and performance tests |
| `examples/dd`, `examples/ipi` | C and C++ examples |
| `docs/dd`, `docs/ipi` | Doxygen configuration for the reference documentation |
| `tools/dd` | Command line tools, such as `js-snippet-export`, which exports the JavaScript snippets in a data file |
| `VisualStudio` | Visual Studio solutions for each product |
| `device-detection-data`, `ip-intelligence-data` | Data file submodules, used by the tests and examples |
| `ci` | The scripts the shared 51Degrees CI workflows call |

## Getting the code and data

Clone with the submodules:

```
git clone --recurse-submodules https://github.com/51Degrees/51degrees-cxx.git
```

If you have already cloned the repository, fetch them with:

```
git submodule update --init
```

The `device-detection-data` submodule holds a free Lite data file, stored with
[Git LFS](https://git-lfs.github.com/). Install Git LFS (`sudo apt-get install
git-lfs` then `git lfs install` on Linux) and run `git lfs pull` in that
folder. The IP Intelligence Lite file is downloaded as described in the
[ip-intelligence-data README](ip-intelligence-data/README.md). The tests and
examples find the data files by searching upward from where they run for a
folder with one of these two names.

A zip download of this repository contains neither the submodules nor the LFS
files, so each has to be downloaded separately.

## Build tools

On Windows you need Visual Studio 2019 or later, or the
[C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/),
with Platform Toolset `v142` and Windows SDK `10.0.18362.0` or later.

On Linux and macOS you need [CMake 3.24](https://cmake.org/) or later and a
compiler that supports C11 and C++17. Linux builds link `libatomic`, which GCC
5 and later include.

## Building with CMake

From the repository root:

```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

On x64 Windows add `-A x64` to the `cmake ..` command, as CMake builds for
Win32 by default. Libraries go to `build/lib` and executables, examples and
tests included, go to `build/bin`.

These options choose what is built:

| Option | Default | Effect |
| --- | --- | --- |
| `FIFTYONE_BUILD_DEVICE_DETECTION` | `ON` | Device Detection libraries, examples and tests |
| `FIFTYONE_BUILD_IP_INTELLIGENCE` | `ON` | IP Intelligence libraries, examples and tests |
| `BUILD_TESTING` | `ON` | All tests. Turn it off when cross compiling, for example arm64 on an x64 Mac, as the tests cannot run |
| `FIFTYONE_COMMON_CXX_BUILD_TESTING` | `ON` | The common library's own tests |

The compile options of the common library, `MemoryOnly`, `NoThreading` and
`ExceptionsDisabled`, apply to the whole build and are described in the
[common library README](src/common/README.md#build-options). IP Intelligence
always builds its own copy of common with large data file support and the
reduced file layout, so its libraries link `fiftyone-common-c-ipi` and
`fiftyone-common-cxx-ipi`.

When this repository is the top level build, that copy also has its own linked
C names, so one C program can link both products, and
[`test/both`](test/both) is the smallest example. A project that includes this
CMake file keeps the original names unless it sets
`-DFIFTYONE_IPI_SYMBOLS=ON`.

Another CMake project can build these libraries as part of its own build by
including this repository's `CMakeLists.txt`, with the options above set
first.

## Building with Visual Studio

CMake in an MSVC environment produces a Visual Studio solution, `51Degrees`,
with every library, example and test. There are also dedicated solutions:

- `VisualStudio/Common/FiftyOne.Common.sln`
- `VisualStudio/DeviceDetection/DeviceDetection.sln`
- `VisualStudio/IpIntelligence/IpIntelligence.sln`

Besides `Debug` and `Release` they have `MemoryOnly` configurations, which hold
the data in continuous memory for the best performance, and `Single`
configurations, which do not support multi threaded use.

## Tests

The tests use the [Google Test framework](https://github.com/google/googletest),
which CMake downloads. Run every test from the build folder with:

```
ctest -C Release
```

Each product's tests carry a label, so `ctest -C Release -L dd` runs only the
Device Detection tests, and `-L ipi` and `-L common` the others. The common
tests run twice, once against each copy of the common library, and the second
run's names start `Ipi.`.

The Visual Studio solutions get Google Test from NuGet. For the tests to appear
in the Test Explorer, install the
[Test Adapter for Google Test](https://marketplace.visualstudio.com/items?itemName=VisualCPPTeam.TestAdapterforGoogleTest).

### Code coverage

A `Debug` build with gcc or clang adds copies of the libraries ending `-cov`
that are built for coverage, and links the tests to them. After running the
tests, install [gcovr](https://gcovr.com/) 7.2 or later (`pip3 install gcovr`)
and generate a report from the build folder:

```
mkdir -p coverage; gcovr -r .. --html-details -o coverage/coverage.html
```

## History

This repository brings together `common-cxx`, `device-detection-cxx`,
`ip-intelligence-cxx` and `ip-graph-cxx`. Their full history is included, with
every commit keeping its original SHA, so `git log --follow` traces a file
back through its move into this layout. The tags and releases of the original
repositories stay with those repositories.
[docs/path-map.md](docs/path-map.md) lists where each of their paths is now.

## Licence

[European Union Public Licence v1.2](LICENSE)
