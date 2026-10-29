# 51Degrees Common Code Library

![51Degrees](https://51degrees.com/img/logo.png?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-common-code-library "Data rewards the curious") **51Degrees Common C Code**

[Reference Documentation](https://51degrees.com/documentation/apis/device-detection-cxx/index.html?utm_source=github&utm_medium=readme&utm_campaign=51degrees-cxx&utm_content=readme.md&utm_term=51degrees-common-code-library "Reference documentation")

# Introduction

The 51Degrees Common Code Library groups common methods and structures used in 51Degrees APIs. This includes methods to stream from files, data sets, collections of items, arrays, memory management among other things. Anything which is not specific to a functional API is contained here as generic implementations which can be reused in multiple specific use cases.

# Naming Conventions

All public methods and structures are prefixed by `fiftyoneDegrees` then the name of the area they belong to, e.g. the `fiftyoneDegreesCollection` prefix indicates it exists in `collection.c/h`.

The next part of a method name is the verb which describes what the method does, common terms are:

| Verb | Meaning |
|-|-|
| Init | Initializes a structure which has already been allocated. This will take a pointer to the structure which is to initialized. |
| Create | Allocates and initializes a new structure, then returns the pointer. |
| Size | Returns the number of bytes which will be allocated by the accompanying `create` or `init` method. Implicitly, `create` or `init` methods which have no accompanying `size` method do not allocate any memory. |
| Get | Returns a value or pointer which may need to be released after use. |
| Add | Add an item to a structure, this may or may not allocate memory. |
| Process | Processes data and populates a structure which is passed in. |
| Free | Frees the memory allocated to a structure. |
| Release | Releases a resource back to the resource manager or whatever structure in charge of handing out references to that particular resource. |

e.g. the `fiftyoneDegreesDataSetGet` returns a pointer to a data set which will need to be released with the `fiftyoneDegreesDataSetRelease`
method.

Finally, the optional last part completes the description of the method's use, e.g. `fiftyoneDegreesDataSetInitFromMemory` or `fiftyoneDegreesDataSetInitFromFile`.

# Installing

## Dependencies

When compiling on Linux, there is a dependency on `libatomic`. This
is usually already present as part of the C libraries, and is included
as part of GCC 5 and above. On other systems, can be installed with:

```
sudo apt-get install libatomic1
```

## Building

The common library is built with the products in this repository. Building
with CMake or Visual Studio is described in the
[repository README](../../README.md).

## Build Options

### MemoryOnly

In memory only operation compiling without stream capabilities using the `FIFTYONE_DEGREES_MEMORY_ONLY` directive results in performance improvements. By removing the unnecessary jumps to methods which are only used in stream operation, the performance of in memory collections is improved.

The option is enabled using the `FIFTYONE_DEGREES_MEMORY_ONLY` compile flag, which can also be set through CMake using the `-DMemoryOnly=YES` option, or through Visual Studio using the the `Debug-MemoryOnly` or `Release-MemoryOnly` build configurations.

### NoThreading

In single threaded operation compiling without threading using the `FIFTYONE_DEGREES_NO_THREADING` directive results in performance improvements. By removing logic needed to keep operations thread safe, the performance of many operations are improved.

The option is enabled using the `FIFTYONE_DEGREES_NO_THREADING` compile flag, which can also be set through CMake using the `-DNoThreading=YES` option, or through Visual Studio using the `Debug-Single` or `Release-Single` build configurations.

<p style="color:#FF6000">WARNING: This option should only be enabled in single threaded environments, as it means operations will not be thread safe.</P>

### ExceptionsDisabled

Exceptions are handled in the common library using the `fiftyoneDegreesException` structure. This logic can be disabled using the `FIFTYONE_DEGREES_EXCEPTIONS_DISABLED` directive, resulting in performance improvements.

The options is enabled using the `FIFTYONE_DEGREES_EXCEPTIONS_DISABLED` compile flag, which can also be set through CMake using the `-DExceptionsDisabled=YES` option.

<p style="color:#FF6000">WARNING: When exception handling is disabled, using the functionality in this library incorrectly can result in a segmentation fault instead of an exception being set.<p>

### LargeDataFileSupport

Data files larger than 4 GB need 64 bit file offsets, which the
`FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT` directive enables. In CMake the
`-DLargeDataFileSupport=YES` option sets it for the copy of the library that
Device Detection uses, and in Visual Studio the `LargeDataFiles`
configurations set it with the reduced file layout below. IP Intelligence
always builds its copy with both.

### ReducedFile

A reduced size data file has the following differences:
- `profileId` is removed from a `fiftyoneDegreesProfile`,
- `fiftyoneDegreesProfile->valueCount` is 2 bytes instead of 4,
- `descriptionOffset` is removed from `fiftyoneDegreesValue`.

Enabling this option makes the API compatible with a reduced size 51Degrees data file. A few methods 
which use the removed fields cannot be implemented with this option, so an exception is set.

# Tests

The common tests are in [`test/common`](../../test/common). They run as
`CommonTests`, and again as `CommonTestsIpi` against the copy of the library
that IP Intelligence uses. Turn off `FIFTYONE_COMMON_CXX_BUILD_TESTING` to skip
both. Running tests and code coverage are described in the
[repository README](../../README.md#tests).

# Referencing the library

## CMake

When building using CMake, static libraries are built in stages. These can be included in an executable just as they are in the examples. If these are included through a CMake file, the dependencies will be resolved automatically. However, if linking another way, all dependencies will need to be included from the `lib/` directory. For example, to use the Common C library, the `fiftyone-common-c` static library would need to be included, and for C++, `fiftyone-common-cxx` is needed in addition.

## Visual Studio

The Visual Studio solution contains static libraries which have all the dependencies set up correctly, so referencing these in a Visual Studio solution should be fairly self explanatory.
