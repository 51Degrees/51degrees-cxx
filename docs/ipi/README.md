# 51Degrees Ip Intelligence API

![51Degrees](https://raw.githubusercontent.com/51Degrees/51degrees-cxx/main/.github/images/logo-160.png "Data rewards the curious") **IP Intelligence in C**

[C Documentation](https://51degrees.com/documentation/apis/ip-intelligence-cxx/group___fifty_one_degrees_ip_intelligence_api.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-ip-intelligence-api) and the [C++ Documentation](https://51degrees.com/documentation/apis/ip-intelligence-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-ip-intelligence-api).

The 51Degrees IP intelligence API is built on the 51Degrees [common API](../../src/common/README.md).

## Pre-requisites

### Data file

In order to perform IP intelligence, you will need to obtain a 51Degrees data file.  The free Lite data file is downloaded by `data/get-data-files.sh` or `data/get-data-files.ps1`, in the [data](https://github.com/51Degrees/data) submodule. To obtain an Enterprise data file please [contact us](https://51degrees.com/contact-us?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=data-file).

By default, the data file has to be in the `data/ip-intelligence` folder of this repository for the tests and examples to work, which is where the scripts save it. The tests also need the free ASN data file, `51Degrees-IPIV4AsnIpiV41.ipi`, which the scripts download as well.

The examples resolve the location of the data file in the following order:

1. A path supplied as a command line argument to the example.
2. The `51DEGREES_IPI_PATH` environment variable containing an explicit path to the data file.
3. A search of the parent folder structure for the example's default file in the `data/ip-intelligence` folder, where the scripts save the free data files. Most examples default to the free Lite data file. The performance and reload examples default to the far smaller free ASN data file, as any data file will do for them. When the file is missing there, the example runs the data repository's script to download it.

On a free data file, the examples show the properties only the Enterprise data file has as needing it, with a link to ask for a copy. The country overlap example needs properties only the Enterprise data file has, so on a free data file it says which, gives the link and stops.

Note that on Linux shells the `51DEGREES_IPI_PATH` variable name starts with a digit, so it must be set with the `env` command, for example `env 51DEGREES_IPI_PATH=/path/to/file.ipi ./GettingStarted`, rather than a plain `export`.

## Building and testing

Cloning, building with CMake or Visual Studio, and running the tests are the
same for every product in this repository, and are described in the
[repository README](../../README.md).

## Memory and the required properties

Two settings of the configuration, `fiftyoneDegreesConfigIpi` in C and `ConfigIpi` in C++, hold in memory what the required properties read. Each preset sets both, and any configuration can set either.

`loadRequiredPropertiesOnly` (`setLoadRequiredPropertiesOnly()` in C++) applies when the required properties name some but not all of the properties in the data file. Only what they read is then loaded into memory:

- The graphs of the components the named properties belong to, each following the graph setting. The graphs of other components read from the file.
- The profile offsets, profile groups and profiles of those components.
- The values of the named properties, with their default values.
- The strings of those values, leaving on file any stretch of at least 64 KiB that no named property reads. For example, the polygons of `Areas` stay on file unless `Areas` is named.

Each part is held in whole records, and the rest of those collections reads from the file when needed, with no cache. The components, maps, properties, property types and the collection that lists the graphs follow their own settings. Anything not held reads from the file, so it still works, more slowly and allocating memory. That covers a direct graph call for a component with no named property, and meta data for what was not named. A property that was not named gives no value, as it does with every other configuration. The setting has no effect when no property or every property is named, or when the whole data file is in memory.

`defaultValuesInMemory` (`setDefaultValuesInMemory()` in C++) holds the default value of each required property in memory when the data set is created, with what reading it takes from the strings and the property types. A lookup reads the default of each mandatory property its profile has no value for, so holding the defaults means those reads take no lock and read nothing from the file. Nothing is copied from a collection that is already in memory, so the setting has no effect where the values, strings and property types are all loaded, as in HighPerformance with no properties named. It also has no effect when the whole data file is in memory, or when `loadRequiredPropertiesOnly` applies, as both already hold the default values.

The presets set them as follows. Choosing a preset in C++ replaces both settings with the preset's.

| Preset | `loadRequiredPropertiesOnly` | `defaultValuesInMemory` |
| --- | --- | --- |
| InMemory (`setMaxPerformance()`) | false | true, with no effect |
| HighPerformance | true | true |
| Balanced, BalancedTemp and the default | false | true |
| LowMemory | false | true |

The HighPerformance configuration (`fiftyoneDegreesIpiHighPerformanceConfig`, or `setHighPerformance()` in C++) loads the data file into memory, so with some properties named it loads only what they read and a lookup that reads them reads nothing from the file. Naming no properties, or every one, loads everything.

The Balanced configuration (`fiftyoneDegreesIpiBalancedConfig`, or `setBalanced()` in C++) holds the strings, the graphs, the profile offsets and the property types in memory, and reads values, profiles and profile groups through caches of 100,000 values, 20,000 profiles and 50,000 profile groups. The default value of each required property is held in memory too, and the profile of each result is read through the cache once for each lookup rather than once for each property, which uses 512 bytes for each component in each set of results. Naming the required properties does not change what Balanced holds. The C API uses Balanced when no configuration is given.

The LowMemory configuration (`fiftyoneDegreesIpiLowMemoryConfig`, or `setLowMemory()` in C++) holds only the default values in memory, and reads everything else from the file with no cache.

## Address forms

An IPv4-mapped IPv6 address (`::ffff:a.b.c.d`) is looked up as the IPv4 address `a.b.c.d`, whether it is given as text in any form, as evidence or as bytes. Every property and the target address are then those of the IPv4 address.

Every other IPv6 address is looked up as an IPv6 address, including those that hold an IPv4 address. An IPv4-embedded address from [RFC 6052](https://www.rfc-editor.org/rfc/rfc6052.html), such as `64:ff9b::a.b.c.d`, stays an IPv6 address, because deriving the IPv4 address from it is the job of an address translator ([RFC 6052 section 1.3](https://www.rfc-editor.org/rfc/rfc6052.html#section-1.3)).

An address below the lowest key in a graph has no profile for that graph's component. A property of that component then reads as its default if it is mandatory with a default value, and otherwise has no value.

The [IP Intelligence specification](https://github.com/51Degrees/specifications/blob/main/ip-intelligence-specification/pipeline-elements/ip-intelligence-on-premise.md#address-forms) sets out each form.

## Reading values

When the properties were named, in C through the required properties and in C++ through the properties the engine is created with, looking up an address walks the graph of each component a named property belongs to, and no other, so no read walks a graph. When no properties were named, looking up an address walks no graph, and the graph of a component is walked the first time one of its properties is read. Either way the values are the same whatever is read first.

## Threads

A set of results is used by one thread at a time, because reading a value writes into the results and, when no properties were named, the first read of a property of a component walks the component's graph. A program that looks up addresses on several threads uses a set of results for each thread. The resource manager, or in C++ the engine, can be shared by every thread, unless the library is built with the `NoThreading` option described in the [common library README](../../src/common/README.md#nothreading).

## Referencing the API

### Adding references with CMake

When building using CMake, static libraries are built in stages. These can be included in an executable just as they are in the examples. If these are included through a CMake file, the dependencies will be resolved automatically. However, if linking another way, all dependencies will need to be included from the `lib/` directory. For example, to use the IP Intelligence C API, the following static libraries would need to be included:

- `fiftyone-common-c-ipi`
- `fiftyone-ip-intelligence-c`

and for the IP Intelligence C++ API, the following are needed in addition:

- `fiftyone-common-cxx-ipi`
- `fiftyone-ip-intelligence-cxx`

The CMake project also builds a single shared library containing everything. This is named `fiftyone-ip-intelligence-complete` and can be referenced by any C or C++ project.

A project that uses an installed copy of the libraries finds them with
`find_package(51Degrees CONFIG REQUIRED)` and links
`51Degrees::ip-intelligence-c` or `51Degrees::ip-intelligence-cxx`, which bring
the libraries above and the definitions their copy of common is compiled with.
The headers are then included as, for example, `<51degrees/ipi/ipi.h>`.
[Installing](../../README.md#installing) in the repository README says how to
install them.

### Adding references with Visual Studio

The Visual Studio solution contains static libraries which have all the dependencies set up correctly, so referencing these in a Visual Studio solution should be fairly self explanatory.

## Examples

There are several examples available, the source files can be found in the [`examples/ipi`](../../examples/ipi) folder. The examples are written in C or CPP.

### ⚠️ Required files

See the [data repository README](https://github.com/51Degrees/data#get-the-data-files) on how to download the necessary files.

### Running examples in Visual Studio

All the examples are available to run in the `VisualStudio/IpIntelligence/IpIntelligence.sln` solution.

### IP Intelligence Examples

|Example|Description|Language|
|-------|-----------|--------|
|Getting Started|This example shows how to get set up with an IP Intelligence engine and begin using it to process IP addresses.|C / C++|
|Country Overlap|This example measures how often the likely area of an IPv4 address crosses a country border, and writes the results to a CSV file. It needs the Enterprise data file.|C|
|Find Profiles|This example iterates over every profile in the data file that has a given property value, such as a registered country.|C|
|Meta Data|This example shows how to interrogate the meta data associated with the contents of an IP Intelligence data file.|C++|
|MemIpi|This example measures the memory usage of the IP Intelligence process.|C|
|Offline Processing|This example shows how process data for later viewing using an IP Intelligence data file.|C|
|PerfIpi|Command line performance evaluation program which takes a file of IP addresses and returns a performance score measured in lookups per second per CPU core.|C|
|Performance|This example times looking up the IP addresses in an evidence file, first for a few properties and then for every property.|C|
|ProcIpi|Command line process which takes an IP address via stdin and return IP intelligence properties via stdout.|C|
|Reload From File|This example illustrates how to reload the data file from the data file on disk without restarting the application.|C / C++|
|Reload From Memory|This example illustrates how to reload the data file from a continuous memory space that the data file was read into without restarting the application.|C / C++|
|Strongly Typed|This example takes some IP addresses and returns the Longitude and Latitude properties as numbers, and the Areas property, which all need the Enterprise data file.|C++|
