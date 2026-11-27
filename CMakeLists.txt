cmake_minimum_required(VERSION 3.24)

# Builds common, Device Detection and IP Intelligence, with their examples and
# tests, from one configure. Turn a product off to build only the other:
#
#   cmake -S . -B build -D_51DEGREES_BUILD_IP_INTELLIGENCE=OFF
#
# This file can also be included by another project, which then gets the
# same targets.

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Minimum required versions for MSVC
set(MIN_VS_TOOLSET_VERSION 142)
set(MIN_VS_PLATFORM_VERSION 10.0.18362.0)

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.1)
	message(FATAL_ERROR "Require at least gcc-5.1")
endif()

# Opt-in AddressSanitizer. When ON, applied to compile + link of every target
# so library code is instrumented, not only the test binary runtime, which
# ASAN needs to report use-after-free instead of a bare SEGV. Default OFF so
# production package builds stay ASAN-free.
option(_51DEGREES_WITH_ASAN "Build with AddressSanitizer (compile + link)" OFF)

# Opt-in ThreadSanitizer, applied in the same way. It can't be combined with
# AddressSanitizer in one build. Debug builds count test coverage, and the
# counters are updated atomically so that ThreadSanitizer doesn't report them
# as data races.
option(_51DEGREES_WITH_TSAN "Build with ThreadSanitizer (compile + link)" OFF)
if (_51DEGREES_WITH_TSAN AND _51DEGREES_WITH_ASAN)
	message(FATAL_ERROR
		"_51DEGREES_WITH_TSAN and _51DEGREES_WITH_ASAN can't both be ON")
endif()

include(CMakeDependentOption)

# The version the installed package reports. A release is a tag rather than a
# version in a file, so a build from a source archive is given it.
set(_51DEGREES_VERSION "" CACHE STRING "Version of the installed package")
if (NOT _51DEGREES_VERSION AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/.git)
	execute_process(
		COMMAND git describe --tags --abbrev=0 --match "[0-9]*.[0-9]*.[0-9]*"
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE _51DEGREES_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
endif()
if (NOT _51DEGREES_VERSION)
	set(_51DEGREES_VERSION 0.0.0)
endif()

project(51Degrees VERSION ${_51DEGREES_VERSION} LANGUAGES CXX C)

# Settings for every target, here and for coverage below, are added to
# CMAKE_C_FLAGS and CMAKE_CXX_FLAGS. CMake uses them to compile and to link,
# and they also reach a project that includes this file. CMake 4.5 and later
# report add_compile_options, add_link_options and the other directory-wide
# commands as an author warning.
if (_51DEGREES_WITH_ASAN AND NOT CMAKE_HOST_WIN32)
	string(APPEND CMAKE_C_FLAGS " -fsanitize=address")
	string(APPEND CMAKE_CXX_FLAGS " -fsanitize=address")
elseif (_51DEGREES_WITH_TSAN AND NOT CMAKE_HOST_WIN32)
	string(APPEND CMAKE_C_FLAGS " -fsanitize=thread -fprofile-update=atomic")
	string(APPEND CMAKE_CXX_FLAGS " -fsanitize=thread -fprofile-update=atomic")
endif()

option(_51DEGREES_BUILD_DEVICE_DETECTION "Build Device Detection" ON)
option(_51DEGREES_BUILD_IP_INTELLIGENCE "Build IP Intelligence" ON)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
	set(_51DEGREES_TOP_LEVEL ON)
else()
	set(_51DEGREES_TOP_LEVEL OFF)
endif()

# Gives IP Intelligence's copy of common its own linked names, which one program
# linking both products needs. A project that includes this file keeps the
# original names unless it turns this on, so language wrappers don't change.
option(_51DEGREES_IPI_SYMBOLS
	"Give IP Intelligence's copy of common its own linked names"
	${_51DEGREES_TOP_LEVEL})
# The same for the common C++ classes. Off by default because the wrappers SWIG
# generates have not been checked with the changed linked names.
option(_51DEGREES_CXX_BOTH_PRODUCTS
	"Give IP Intelligence's copy of the common C++ classes its own linked names" OFF)
# Adds the install rules in cmake/install.cmake. A project that includes this
# file installs none of it unless it turns this on.
option(_51DEGREES_INSTALL "Add the install rules" ${_51DEGREES_TOP_LEVEL})

# A Visual Studio generator names the toolset and the Windows SDK. Another
# generator, such as Ninja, gives only the compiler's toolset version.
if (MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
	# Check minimum required versions for MSVC
	if (CMAKE_VS_PLATFORM_TOOLSET MATCHES "v([0-9]+)")
		if (${CMAKE_MATCH_1} LESS MIN_VS_TOOLSET_VERSION)
			message(FATAL_ERROR "Require at least Platform Toolset v${MIN_VS_TOOLSET_VERSION}. These can be installed by Visual Studio Installer or as part of C++ Build Tools which can be downloaded from https://visualstudio.microsoft.com/visual-cpp-build-tools/")
		endif()
	else()
		message(FATAL_ERROR "Platform toolset version should start with 'v' and followed by numbers. Got ${CMAKE_VS_PLATFORM_TOOLSET}.")
	endif()

	if (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS ${MIN_VS_PLATFORM_VERSION})
		message(FATAL_ERROR "Require at least windows SDK version ${MIN_VS_PLATFORM_VERSION} which can be downloaded from https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/")
	endif()
elseif (MSVC AND MSVC_TOOLSET_VERSION LESS MIN_VS_TOOLSET_VERSION)
	message(FATAL_ERROR "Require at least Platform Toolset v${MIN_VS_TOOLSET_VERSION}. These can be installed by Visual Studio Installer or as part of C++ Build Tools which can be downloaded from https://visualstudio.microsoft.com/visual-cpp-build-tools/")
endif()

include(CTest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(Threads REQUIRED)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
	set(IS_ARM TRUE)
endif()

if (NOT CMAKE_BUILD_TYPE AND NOT MSVC)
	message("-- Setting default CMAKE_BUILD_TYPE  to Release")
	set(CMAKE_BUILD_TYPE "Release")
endif()

# Compute is test coverage enabled
string(TOLOWER "${CMAKE_BUILD_TYPE}" lower_CMAKE_BUILD_TYPE)
if (NOT MSVC AND "${lower_CMAKE_BUILD_TYPE}" STREQUAL "debug")
	set(TESTCOVERAGE_ENABLED TRUE)
else()
	set(TESTCOVERAGE_ENABLED FALSE)
endif()
message("-- TESTCOVERAGE_ENABLED=${TESTCOVERAGE_ENABLED}")

# The tests run on several threads, so GCC updates the coverage counters
# atomically. Otherwise increments are lost and gcovr reads negative counts.
if (TESTCOVERAGE_ENABLED AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
	string(APPEND CMAKE_C_FLAGS " -fprofile-update=prefer-atomic")
	string(APPEND CMAKE_CXX_FLAGS " -fprofile-update=prefer-atomic")
endif()

if (NOT MSVC)
	if (lower_CMAKE_BUILD_TYPE MATCHES "^debug")
		set(COMPILE_OPTION_DEBUG -D_DEBUG)
	endif()
endif()

option(_51DEGREES_32BIT "Build 32 bit libraries and programs" OFF)
option(_51DEGREES_MEMORY_ONLY "Build without the code that reads from a file" OFF)
option(_51DEGREES_NO_THREADING "Build without threading" OFF)
option(_51DEGREES_EXCEPTIONS_DISABLED "Build without exceptions" OFF)
# These two apply to the copy of common that Device Detection uses. IP
# Intelligence always builds its own copy with both on.
option(_51DEGREES_LARGE_DATA_FILE_SUPPORT "Build with support for data files over 4GB" OFF)
option(_51DEGREES_REDUCED_FILE "Build for a reduced data file" OFF)

if (_51DEGREES_32BIT AND NOT IS_ARM)
	message("-- 32 bit compilation")
	if (NOT MSVC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
	else()
		set(CMAKE_VS_PLATFORM_NAME "Win32")
	endif()
endif()

if (NOT MSVC AND NOT APPLE)
	find_package(GccAtomic)
	set(EXE_LINKER_FLAGS "${EXE_LINKER_FLAGS} -latomic")
endif()

if (_51DEGREES_MEMORY_ONLY)
	message("-- Memory only compilation (FIFTYONE_DEGREES_MEMORY_ONLY) is enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIFTYONE_DEGREES_MEMORY_ONLY")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFIFTYONE_DEGREES_MEMORY_ONLY")
endif()

if (_51DEGREES_NO_THREADING)
	message("-- No Threading compilation (FIFTYONE_DEGREES_NO_THREADING) is enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIFTYONE_DEGREES_NO_THREADING")
endif()

if (_51DEGREES_EXCEPTIONS_DISABLED)
	message("-- Exceptions disable compilation (FIFTYONE_DEGREES_EXCEPTIONS_DISABLED) is enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIFTYONE_DEGREES_EXCEPTIONS_DISABLED")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFIFTYONE_DEGREES_EXCEPTIONS_DISABLED")
endif()

if (MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _UNICODE /D UNICODE")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# BUILD_TESTING is true by default, but in some cases it is desirable to turn
# off the common tests while keeping the product tests.
cmake_dependent_option(_51DEGREES_COMMON_CXX_BUILD_TESTING "" ON "BUILD_TESTING" OFF)
message("-- _51DEGREES_COMMON_CXX_BUILD_TESTING=${_51DEGREES_COMMON_CXX_BUILD_TESTING}")
# The common tests run a second time against the copy of common that IP
# Intelligence uses. Its file offsets are 64 bit, so only on 64 bit builds.
cmake_dependent_option(_51DEGREES_COMMON_CXX_BUILD_TESTING_IPI
	"Also test the copy of common that IP Intelligence uses" ON
	"_51DEGREES_COMMON_CXX_BUILD_TESTING;CMAKE_SIZEOF_VOID_P EQUAL 8" OFF)
# The tests compile the examples' code and run the tools, so both are built
# whenever the tests are.
cmake_dependent_option(_51DEGREES_BUILD_EXAMPLES
	"Build the examples and the command line tools" ON
	"NOT BUILD_TESTING" ON)

if (BUILD_TESTING)
	include(FetchContent)
	FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/58d77fa8070e8cec2dc1ed015d66b454c8d78850.zip) # release-1.12.1
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	# googletest asks for CMake 3.5, which CMake 3.31 and later warn is
	# deprecated. It is built with the policies of the CMake version this
	# project asks for instead.
	set(CMAKE_POLICY_VERSION_MINIMUM 3.24)
	FetchContent_MakeAvailable(googletest)
	unset(CMAKE_POLICY_VERSION_MINIMUM)
	# googletest marks its include folders as system folders, and compilers
	# report nothing from a system header. That hides the warnings the
	# assertions raise on the tests' own values, such as EXPECT_EQ comparing
	# signed with unsigned. They are made ordinary include folders so those
	# warnings show. googletest's own libraries keep googletest's options.
	foreach(target gtest gtest_main gmock gmock_main)
		set_property(TARGET ${target}
			PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "")
	endforeach()
	include(GoogleTest)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/src/common/CMakeLists.txt NO_POLICY_SCOPE)
if (_51DEGREES_COMMON_CXX_BUILD_TESTING)
	include(${CMAKE_CURRENT_LIST_DIR}/test/common/CMakeLists.txt NO_POLICY_SCOPE)
endif()

if (_51DEGREES_BUILD_DEVICE_DETECTION)
	include(${CMAKE_CURRENT_LIST_DIR}/src/dd/CMakeLists.txt NO_POLICY_SCOPE)
	if (_51DEGREES_BUILD_EXAMPLES)
		include(${CMAKE_CURRENT_LIST_DIR}/examples/dd/CMakeLists.txt NO_POLICY_SCOPE)
		include(${CMAKE_CURRENT_LIST_DIR}/tools/dd/js-snippet-export/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
	if (BUILD_TESTING)
		include(${CMAKE_CURRENT_LIST_DIR}/test/dd/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
endif()

if (_51DEGREES_BUILD_IP_INTELLIGENCE)
	include(${CMAKE_CURRENT_LIST_DIR}/src/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	if (_51DEGREES_BUILD_EXAMPLES)
		include(${CMAKE_CURRENT_LIST_DIR}/examples/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
	if (BUILD_TESTING)
		include(${CMAKE_CURRENT_LIST_DIR}/test/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
endif()

if (BUILD_TESTING AND _51DEGREES_BUILD_DEVICE_DETECTION AND
	_51DEGREES_BUILD_IP_INTELLIGENCE AND _51DEGREES_IPI_SYMBOLS)
	include(${CMAKE_CURRENT_LIST_DIR}/test/both/CMakeLists.txt NO_POLICY_SCOPE)
endif()

if (_51DEGREES_INSTALL)
	include(${CMAKE_CURRENT_LIST_DIR}/cmake/install.cmake NO_POLICY_SCOPE)
endif()
