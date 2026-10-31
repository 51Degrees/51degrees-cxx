# 51Degrees Ip Intelligence API

![51Degrees](https://51degrees.com/img/logo.png?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-ip-intelligence-api "Data rewards the curious") **IP Intelligence in C**

[C Documentation](https://51degrees.com/documentation/apis/ip-intelligence-cxx/index.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-ip-intelligence-api) and the [C++ Documentation](https://51degrees.com/documentation/apis/ip-intelligence-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-ip-intelligence-api).

The 51Degrees IP intelligence API is built on the 51Degrees [common API](../../src/common/README.md).

## Pre-requisites

### Data file

In order to perform IP intelligence, you will need to obtain a 51Degrees data file.  Lite data file can be obtained for free as specified in the [`ip-intelligence-data/README`](https://github.com/51Degrees/ip-intelligence-data/). To obtain an Enterprise data file please [contact us](https://51degrees.com/contact-us?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=data-file).

By default, the downloaded data file has to be placed in the 'ip-intelligence-data' folder at the root of this repository for the tests and examples to work.

The examples resolve the location of the data file in the following order:

1. A path supplied as a command line argument to the example.
2. The `51DEGREES_IPI_PATH` environment variable containing an explicit path to the data file.
3. A search of the parent folder structure for the expected file name in the 'ip-intelligence-data' folder, where the free 'Lite' data file is expected to be placed.

Note that on Linux shells the `51DEGREES_IPI_PATH` variable name starts with a digit, so it must be set with the `env` command, for example `env 51DEGREES_IPI_PATH=/path/to/file.ipi ./GettingStarted`, rather than a plain `export`.

## Building and testing

Cloning, building with CMake or Visual Studio, and running the tests are the
same for every product in this repository, and are described in the
[repository README](../../README.md).

## Memory and the required properties

The Balanced configuration (`fiftyoneDegreesIpiBalancedConfig`, or `setBalanced()` in C++) holds the strings, the graphs, the profile offsets and the property types in memory, and reads values and profiles through caches. The C API uses it when no configuration is given.

Naming the required properties is all it takes to get more from it. There is no other setting. When fewer properties are named than the data file has, the parts of the file they do not read stay on file:

- The graphs of components with no required property.
- Stretches of strings of at least 64 KiB that no required property reads. For example, the polygons of `Areas` stay on file unless `Areas` is named.

The memory that frees is then spent, in this order, as far as it lasts:

1. The values of the required properties, smallest first.
2. The profile groups.
3. The profiles of the components with a required property, smallest first.
4. A larger profile cache, then part of the next component's profiles if at least half of them fit.

The memory used does not rise above what naming nothing uses, and the answers do not change. A caller who names nothing, or every property, gets the configuration as it is.

Anything not held reads from the file, so it still works, more slowly and allocating memory. That covers a direct graph call for a component with no required property, and meta data for what was not named. A property that was not named gives no value, as it does with every other configuration.

## Referencing the API

### Adding references with CMake

When building using CMake, static libraries are built in stages. These can be included in an executable just as they are in the examples. If these are included through a CMake file, the dependencies will be resolved automatically. However, if linking another way, all dependencies will need to be included from the `lib/` directory. For example, to use the IP Intelligence C API, the following static libraries would need to be included:

- `fiftyone-common-c-ipi`
- `fiftyone-ip-intelligence-c`

and for the IP Intelligence C++ API, the following are needed in addition:

- `fiftyone-common-cxx-ipi`
- `fiftyone-ip-intelligence-cxx`

The CMake project also builds a single shared library containing everything. This is named `fiftyone-ip-intelligence-complete` and can be referenced by any C or C++ project.

### Adding references with Visual Studio

The Visual Studio solution contains static libraries which have all the dependencies set up correctly, so referencing these in a Visual Studio solution should be fairly self explanatory.

## Examples

There are several examples available, the source files can be found in the [`examples/ipi`](../../examples/ipi) folder. The examples are written in C or CPP.

### ⚠️ Required files

See `ip-intelligence-data/README` ([local](../../ip-intelligence-data/README.md) / [GitHub](https://github.com/51Degrees/ip-intelligence-data/)) on how to pull and/or generate necessary files.

### Running examples in Visual Studio

All the examples are available to run in the `VisualStudio/IpIntelligence/IpIntelligence.sln` solution.

### IP Intelligence Examples

|Example|Description|Language|
|-------|-----------|--------|
|Getting Started|This example shows how to get set up with an IP Intelligence engine and begin using it to process IP addresses.|C / C++|
|Meta Data|This example shows how to interrogate the meta data associated with the contents of an IP Intelligence data file.|C++|
|MemIpi|This example measures the memory usage of the IP Intelligence process.|C|
|Offline Processing|This example shows how process data for later viewing using an IP Intelligence data file.|C|
|PerfIpi|Command line performance evaluation program which takes a file of IP addresses and returns a performance score measured in lookups per second per CPU core.|C|
|ProcIpi|Command line process which takes an IP address via stdin and return IP intelligence properties via stdout.|C|
|Reload From File|This example illustrates how to reload the data file from the data file on disk without restarting the application.|C / C++|
|Reload From Memory|This example illustrates how to reload the data file from a continuous memory space that the data file was read into without restarting the application.|C / C++|
|Strongly Typed|This example  takes some IP addresses and returns the value of the AverageLocation property as a coordinate which is a pair of float.|C / C++|
