# 51Degrees Device Detection API

![51Degrees](https://51degrees.com/img/logo.png?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-device-detection-api "Data rewards the curious") **Device Detection**

[C Documentation](https://51degrees.com/device-detection-cxx/modules.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-device-detection-api) and the [C++ Documentation](https://51degrees.com/device-detection-cxx/namespaces.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-device-detection-api).

The 51Degrees device detection API is built on the 51Degrees [common API](../../src/common/README.md).

# Pre-requisites

## Data File

In order to perform device detection, you will need to use a 51Degrees data file. 
This repository includes a free, 'lite' file in the 'device-detection-data' 
submodule that has a significantly reduced set of properties. To obtain a 
file with a more complete set of device properties see the 
[51Degrees website](https://51degrees.com/pricing?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=data-file). 
If you want to use the lite file, you will need to install [GitLFS](https://git-lfs.github.com/).

For Linux:
```
sudo apt-get install git-lfs
git lfs install
```

Then, navigate to the device-detection-data directory and execute:

```
git lfs pull
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

## Visual Studio

The Visual Studio solution contains static libraries which have all the dependencies set up correctly, so referencing these in a Visual Studio solution should be fairly self explanatory.

# Pseudo header length limit

A pseudo header joins the values of several HTTP headers into one value, for
example `Sec-CH-UA`, `Sec-CH-UA-Mobile`, `Sec-CH-UA-Platform` and `User-Agent`.
The value is built in working memory that is allocated when a set of results is
created, and detection does not grow that memory. So a pseudo header longer than
the limit is ignored. Detection moves on to the next header, no error is
reported, and the result can rest on less detailed evidence.

The limit is `maxMatchedUserAgentLength` in the device detection configuration,
which defaults to 500. It counts bytes, including one separator between each
value. A single header such as `User-Agent` does not use this memory, so it is
processed whatever its length.

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
