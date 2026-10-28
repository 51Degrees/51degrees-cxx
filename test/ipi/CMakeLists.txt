# IP Intelligence tests, included from the root CMakeLists.txt.

set(COMMON_TEST ${CMAKE_CURRENT_LIST_DIR}/../common)
set(IPI_TEST ${CMAKE_CURRENT_LIST_DIR})

FILE(GLOB IPI_TEST_SRC ${IPI_TEST}/*.cpp)
FILE(GLOB IPI_TEST_H ${IPI_TEST}/*.hpp)

add_executable(IpiTests
	${IPI_TEST_SRC} ${IPI_TEST_H}
	${COMMON_TEST}/Base.cpp
	${COMMON_TEST}/EngineTests.cpp
	${COMMON_TEST}/ExampleTests.cpp
	${EXAMPLES_BASE_SRC} ${EXAMPLES_BASE_H})

if (TESTCOVERAGE_ENABLED)
	target_link_libraries(IpiTests fiftyone-ip-intelligence-cxx-cov gtest_main)
	target_link_options(IpiTests PRIVATE "-fprofile-arcs" "-ftest-coverage")
else()
	target_link_libraries(IpiTests fiftyone-ip-intelligence-cxx gtest_main)
endif()

# DISCOVERY_TIMEOUT must stay outside PROPERTIES; anything after that keyword is
# parsed as a test property. The 5 second default is too tight for Windows CI.
gtest_discover_tests(IpiTests DISCOVERY_TIMEOUT 600 DISCOVERY_MODE PRE_TEST
	PROPERTIES LABELS ipi)

set_target_properties(IpiTests PROPERTIES FOLDER "Tests")

if (MSVC)
	target_compile_options(IpiTests PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
	target_link_options(IpiTests PRIVATE "/WX")
else()
	target_compile_options(IpiTests PRIVATE ${COMPILE_OPTION_DEBUG})
endif()

if (CMAKE_COMPILER_IS_GNUCC)
	target_compile_options(IpiTests PRIVATE "-Wall" "-Werror" "-Wno-unused-variable" "-Wno-unused-result" "-Wno-unused-but-set-variable")
endif()
