cmake_minimum_required(VERSION 3.24)

# Builds common, Device Detection and IP Intelligence, with their examples and
# tests, from one configure. Turn a product off to build only the other:
#
#   cmake -S . -B build -DFIFTYONE_BUILD_IP_INTELLIGENCE=OFF
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
# production package builds stay ASAN-free. Gated on CMAKE_HOST_WIN32 rather
# than MSVC/MINGW because this runs before project() and the compiler
# variables are not yet set.
option(WITH_ASAN "Build with AddressSanitizer (compile + link)" OFF)
if (WITH_ASAN AND NOT CMAKE_HOST_WIN32)
	add_compile_options(-fsanitize=address)
	add_link_options(-fsanitize=address)
endif()

include(CMakeDependentOption)

project(51Degrees VERSION 4.0.1 LANGUAGES CXX C)

option(FIFTYONE_BUILD_DEVICE_DETECTION "Build Device Detection" ON)
option(FIFTYONE_BUILD_IP_INTELLIGENCE "Build IP Intelligence" ON)

# Gives IP Intelligence's copy of common its own linked names, which one program
# linking both products needs. A project that includes this file keeps the
# original names unless it turns this on, so language wrappers don't change.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
	set(FIFTYONE_IPI_SYMBOLS_DEFAULT ON)
else()
	set(FIFTYONE_IPI_SYMBOLS_DEFAULT OFF)
endif()
option(FIFTYONE_IPI_SYMBOLS
	"Give IP Intelligence's copy of common its own linked names"
	${FIFTYONE_IPI_SYMBOLS_DEFAULT})

if (MSVC)
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

if (NOT MSVC)
	if (lower_CMAKE_BUILD_TYPE MATCHES "^debug")
		set(COMPILE_OPTION_DEBUG -D_DEBUG)
	endif()
endif()

option(32bit "32bit" OFF)
option(MemoryOnly "MemoryOnly" OFF)
option(NoThreading "NoThreading" OFF)
option(ExceptionsDisabled "ExceptionsDisabled" OFF)
# These two apply to the copy of common that Device Detection uses. IP
# Intelligence always builds its own copy with both on.
option(LargeDataFileSupport "LargeDataFileSupport" OFF)
option(ReducedFile "ReducedFile" OFF)

if (32bit AND NOT IS_ARM)
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

if (MemoryOnly)
	message("-- Memory only compilation (FIFTYONE_DEGREES_MEMORY_ONLY) is enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIFTYONE_DEGREES_MEMORY_ONLY")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFIFTYONE_DEGREES_MEMORY_ONLY")
endif()

if (NoThreading)
	message("-- No Threading compilation (FIFTYONE_DEGREES_NO_THREADING) is enabled")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIFTYONE_DEGREES_NO_THREADING")
endif()

if (ExceptionsDisabled)
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

if (NOT MSVC)
	include_directories(/usr/local/include)
	link_directories(/usr/local/lib)
endif()

# BUILD_TESTING is true by default, but in some cases it is desirable to turn
# off the common tests while keeping the product tests.
cmake_dependent_option(FIFTYONE_COMMON_CXX_BUILD_TESTING "" ON "BUILD_TESTING" OFF)
message("-- FIFTYONE_COMMON_CXX_BUILD_TESTING=${FIFTYONE_COMMON_CXX_BUILD_TESTING}")
# The common tests run a second time against the copy of common that IP
# Intelligence uses. Its file offsets are 64 bit, so only on 64 bit builds.
cmake_dependent_option(FIFTYONE_COMMON_CXX_BUILD_TESTING_IPI
	"Also test the copy of common that IP Intelligence uses" ON
	"FIFTYONE_COMMON_CXX_BUILD_TESTING;CMAKE_SIZEOF_VOID_P EQUAL 8" OFF)

if (BUILD_TESTING)
	include(FetchContent)
	FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/58d77fa8070e8cec2dc1ed015d66b454c8d78850.zip) # release-1.12.1
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)
	include(GoogleTest)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/src/common/CMakeLists.txt NO_POLICY_SCOPE)
if (FIFTYONE_COMMON_CXX_BUILD_TESTING)
	include(${CMAKE_CURRENT_LIST_DIR}/test/common/CMakeLists.txt NO_POLICY_SCOPE)
endif()

if (FIFTYONE_BUILD_DEVICE_DETECTION)
	include(${CMAKE_CURRENT_LIST_DIR}/src/dd/CMakeLists.txt NO_POLICY_SCOPE)
	include(${CMAKE_CURRENT_LIST_DIR}/examples/dd/CMakeLists.txt NO_POLICY_SCOPE)
	if (BUILD_TESTING)
		include(${CMAKE_CURRENT_LIST_DIR}/test/dd/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
endif()

if (FIFTYONE_BUILD_IP_INTELLIGENCE)
	include(${CMAKE_CURRENT_LIST_DIR}/src/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	include(${CMAKE_CURRENT_LIST_DIR}/examples/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	if (BUILD_TESTING)
		include(${CMAKE_CURRENT_LIST_DIR}/test/ipi/CMakeLists.txt NO_POLICY_SCOPE)
	endif()
endif()

if (BUILD_TESTING AND FIFTYONE_BUILD_DEVICE_DETECTION AND
	FIFTYONE_BUILD_IP_INTELLIGENCE AND FIFTYONE_IPI_SYMBOLS)
	include(${CMAKE_CURRENT_LIST_DIR}/test/both/CMakeLists.txt NO_POLICY_SCOPE)
endif()
