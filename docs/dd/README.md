# 51Degrees Device Detection API

![51Degrees](https://raw.githubusercontent.com/51Degrees/51degrees-cxx/main/.github/images/logo-160.png "Data rewards the curious") **Device Detection**

[C Documentation](https://51degrees.com/documentation/apis/device-detection-cxx/group___fifty_one_degrees_device_detection.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-device-detection-api) and the [C++ Documentation](https://51degrees.com/documentation/apis/device-detection-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-device-detection-api).

The 51Degrees device detection API is built on the 51Degrees [common API](../../src/common/README.md).

# Pre-requisites

## Data File

In order to perform device detection, you will need to use a 51Degrees data file. 
A free, 'lite' file that has a significantly reduced set of properties is 
available through the [data](https://github.com/51Degrees/data) submodule. To obtain a 
file with a more complete set of device properties see the 
[51Degrees website](https://51degrees.com/pricing?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=data-file). 
To download the lite file into `data/device-detection`, run one of these from
the repository root. The examples also download it when it is missing.

```
sh data/get-data-files.sh 51Degrees-LiteV4.1.hash
```

```
./data/get-data-files.ps1 51Degrees-LiteV4.1.hash
```

## Building and testing

Cloning, building with CMake or Visual Studio, and running the tests are the
same for every product in this repository, and are described in the
[repository README](../../README.md).

# Referencing the API

## CMake

When building using CMake, static libraries are built in stages. These can be included in an executable just as they are in the examples. If these are included through a CMake file, the dependencies will be resolved automatically. However, if linking another way, all dependencies will need to be included from the `lib/` directory. For example, to use the Hash C API, the following static libraries would need to be included:
- `fiftyone-common-c`
- `fiftyone-device-detection-c`
- `fiftyone-hash-c`

and for the Hash C++ API, the following are needed in addition:
- `fiftyone-common-cxx`
- `fiftyone-device-detection-cxx`
- `fiftyone-hash-cxx`

The CMake project also builds a single shared library containing everything. This is named `fiftyone-device-detection-complete` and can be referenced by any C or C++ project.

A project that uses an installed copy of the libraries finds them with
`find_package(51Degrees CONFIG REQUIRED)` and links `51Degrees::hash-c` or
`51Degrees::hash-cxx`, which bring the libraries above. The headers are then
included as, for example, `<51degrees/dd/hash/hash.h>`.
[Installing](../../README.md#installing) in the repository README says how to
install them.

## Visual Studio

The Visual Studio solution contains static libraries which have all the dependencies set up correctly, so referencing these in a Visual Studio solution should be fairly self explanatory.

# Header length limit

A pseudo header joins the values of several HTTP headers into one value, for
example `Sec-CH-UA`, `Sec-CH-UA-Mobile`, `Sec-CH-UA-Platform` and `User-Agent`.
The value is built in working memory that is allocated when a set of results is
created, and detection does not grow that memory. A single header such as
`User-Agent` has the same limit. A value longer than the limit is ignored.
Detection moves on to the next header, no error is reported, and the result can
rest on less detailed evidence. If no other header is usable, the results hold
no match, as they do for evidence with no usable header. A direct call with a
`User-Agent` over the limit gives the same results as evidence holding that
value.

The limit is `maxMatchedUserAgentLength` in the device detection configuration,
which defaults to 500. It counts bytes, including one separator between each
value of a pseudo header.

To raise the limit, set a larger value before the data set is created:

| API | Setting |
| --- | --- |
| C | `config.b.maxMatchedUserAgentLength` on the `fiftyoneDegreesConfigHash` used to create the data set |
| C++ | `setMaxMatchedUserAgentLength` on the `ConfigHash` used to create the engine |

Each set of results then holds more memory, made up of:

- the limit plus one byte for pseudo headers
- twice the limit for transforming evidence
- the limit plus one byte for each component, only when
  `updateMatchedUserAgent` is true. Detection also overwrites this memory for
  each result it resets, so a larger limit adds to the time each detection takes

The C examples create a set of results up front and use it for each detection.
`EngineHash::process` in the C++ API creates a new set of results for each call,
so there the memory is allocated once for each call, at the same fixed size.

# Reading values

When the properties were named, in C through the required properties and in
C++ through the properties the engine is created with, populating a set of
results walks the graph of each component a named property belongs to, and
no other, so no read walks a graph and the results keep no copy of the
evidence.

When no properties were named, populating a set of results walks no graph.
The results keep a copy of the evidence, and the graph of a component is
walked the first time one of its properties is read, so a program that reads
the properties of one component walks one graph. The copy is held in memory
the results own, which grows to fit the largest evidence they are populated
from.

Either way the values are the same whatever is read first, and the evidence
can be freed once the results are populated.

The match metrics, such as the iterations and the drift, and the device id
are worked out from every component, so reading them walks every graph not yet
walked. The C++ API does this itself. A C caller that reads the result items
directly calls `fiftyoneDegreesResultsEvaluateAll` first.

# Threads

A set of results is used by one thread at a time, because reading a value
writes into the results and, when no properties were named, the first read of
a property of a component walks the component's graph. A program that detects
on several threads uses a set of results for each thread. The resource
manager, or in C++ the engine, can be shared by every thread, unless the
library is built with the `NoThreading` option described in the
[common library README](../../src/common/README.md#nothreading).

# Examples

There are several examples available, the source files can be found in the [`examples/dd`](../../examples/dd) folder. The examples are written in C or CPP.

### Visual Studio
All the examples are available to run in the `VisualStudio/DeviceDetection/DeviceDetection.sln` solution.

## Device Detection Examples

|Language|Example|Description|
|--|-----------|--------|
|C/C++|GettingStarted|This example shows how to get set up with a Device Detection Cloud aspect engine and begin using it to process User-Agents and Client Hints.<br>Also showcase `query.51D_deviceId` high-priority evidence feature (streamlines profile selection).|
|C/C++|MatchMetrics|This example shows how to view metrics associated with the results of processing with a Device Detection aspect engine using the Hash algorithm.|
|C++|MetaData|This example shows how to interrogate the meta data associated with the contents of a Device Detection Hash data file.|
|C|MemHash|This example measures the memory usage of the Hash algorithm.|
|C|OfflineProcessing|This example shows how process data for later viewing using a Device Detection Hash data file.|
|C|Performance|Command line performance evaluation program which takes a file of user agents and returns a performance score measured in detections per second per CPU core.|
|C|ProcHash|Command line process which takes a user agent via stdin and return device properties via stdout.|
|C/C++|ReloadFromFile|This example illustrates how to reload the data file from the data file on disk without restarting the application.|
|C/C++|ReloadFromMemory|This example illustrates how to reload the data file from a continuous memory space that the data file was read into without restarting the application.|
|C/C++|StronglyTyped|This example  takes some common User-Agents and returns the value of the IsMobile property as a boolean.|
|C|MatchForDeviceId|Retrieve device by deviceId used as evidence. DeviceId may have been obtained previously and stored to later lookup the device properties.|
|C|FindProfiles|Find all profiles that match a certain property value - in this example we count the number of mobile (IsMobile=true) profiles|
