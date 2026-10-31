# Device Detection tests, included from the root CMakeLists.txt.

set(COMMON_TEST ${CMAKE_CURRENT_LIST_DIR}/../common)
set(HASH_TEST ${CMAKE_CURRENT_LIST_DIR}/hash)
set(DD_TEST ${CMAKE_CURRENT_LIST_DIR})

FILE(GLOB DD_TEST_SRC ${DD_TEST}/*.cpp)
FILE(GLOB DD_TEST_H ${DD_TEST}/*.hpp)
FILE(GLOB HASH_TEST_SRC ${HASH_TEST}/*.cpp)
FILE(GLOB HASH_TEST_H ${HASH_TEST}/*.hpp)

add_library(fiftyone-device-detection-test-base
	${DD_TEST_SRC} ${DD_TEST_H}
	${COMMON_TEST}/Base.cpp
	${COMMON_TEST}/EngineTests.cpp
	${COMMON_TEST}/ExampleTests.cpp)
target_link_libraries(fiftyone-device-detection-test-base gtest_main)
if (NOT MSVC)
	target_compile_options(fiftyone-device-detection-test-base PRIVATE ${COMPILE_OPTION_DEBUG})
endif()

set_target_properties(fiftyone-device-detection-test-base PROPERTIES FOLDER "Tests")

add_executable(HashTests
	${HASH_TEST_SRC} ${HASH_TEST_H}
	${CMAKE_CURRENT_LIST_DIR}/../../examples/dd/CPP/Hash/ExampleBase.cpp
	${CMAKE_CURRENT_LIST_DIR}/../../examples/dd/CPP/Hash/ExampleBase.hpp)

if (TESTCOVERAGE_ENABLED)
	target_link_libraries(HashTests fiftyone-device-detection-test-base fiftyone-hash-cxx-cov)
	target_link_options(HashTests PRIVATE "-fprofile-arcs" "-ftest-coverage")
else()
	target_link_libraries(HashTests fiftyone-device-detection-test-base fiftyone-hash-cxx)
endif()

# DISCOVERY_TIMEOUT must stay outside PROPERTIES; anything after that keyword is
# parsed as a test property. The 5 second default is too tight for Windows CI.
gtest_discover_tests(HashTests DISCOVERY_TIMEOUT 600 DISCOVERY_MODE PRE_TEST
	PROPERTIES LABELS dd)

set_target_properties(HashTests PROPERTIES FOLDER "Tests")

if (MSVC)
	target_compile_options(HashTests PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
	target_link_options(HashTests PRIVATE "/WX")
else ()
	target_compile_options(HashTests PRIVATE ${COMPILE_OPTION_DEBUG})
endif()

if (CMAKE_COMPILER_IS_GNUCC)
	target_compile_options(HashTests PRIVATE "-Wall" "-Werror" "-Wno-unused-variable" "-Wno-unused-result" "-Wno-unused-but-set-variable")
	if (MINGW)
		target_compile_options(HashTests PRIVATE "-Wa,-mbig-obj")
	endif()
endif()

# Runs the real tool against the Lite data file and checks what it writes.
add_test(NAME JsSnippetExportTests COMMAND ${CMAKE_COMMAND}
	-DTOOL=$<TARGET_FILE:js-snippet-export>
	-DDATA=${CMAKE_CURRENT_LIST_DIR}/../../device-detection-data/51Degrees-LiteV4.1.hash
	-DWORK=${CMAKE_CURRENT_BINARY_DIR}/js-snippet-export-tests
	-P ${CMAKE_CURRENT_LIST_DIR}/tools/JsSnippetExportTests.cmake)
set_tests_properties(JsSnippetExportTests PROPERTIES LABELS dd)
add_dependencies(HashTests js-snippet-export)
