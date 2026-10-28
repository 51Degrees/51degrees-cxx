# Common tests, included from the root CMakeLists.txt when
# FIFTYONE_COMMON_CXX_BUILD_TESTING is on.

add_executable(CachePerf ${CMAKE_CURRENT_LIST_DIR}/performance/CachePerf.c)
target_link_libraries(CachePerf fiftyone-common-c)
if (MSVC)
	target_compile_options(CachePerf PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
	target_link_options(CachePerf PRIVATE "/WX")
else ()
	target_compile_options(CachePerf PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
endif()
set_target_properties(CachePerf PROPERTIES FOLDER "Examples/Common")

FILE(GLOB COM_TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
FILE(GLOB COM_TEST_H ${CMAKE_CURRENT_LIST_DIR}/*.hpp)

# Adds a common test executable linked to one copy of common. The prefix
# keeps its test names apart from the other copy's.
function(fiftyone_add_common_tests name suffix prefix)
	add_executable(${name} ${COM_TEST_SRC} ${COM_TEST_H})
	if (MSVC)
		target_compile_options(${name} PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
		target_link_options(${name} PRIVATE "/WX")
	else ()
		target_compile_options(${name} PRIVATE ${COMPILE_OPTION_DEBUG})
		if (NOT MINGW)
			target_link_options(${name} PRIVATE "-fsanitize=address")
		endif()
		if (lower_CMAKE_BUILD_TYPE STREQUAL "debug")
			target_link_options(${name} PRIVATE "-fprofile-arcs" "-ftest-coverage")
		endif()
	endif()

	if (TESTCOVERAGE_ENABLED)
		target_link_libraries(${name} fiftyone-common-cxx${suffix}-cov gtest_main)
	else()
		target_link_libraries(${name} fiftyone-common-cxx${suffix} gtest_main)
	endif()

	# DISCOVERY_TIMEOUT must stay outside PROPERTIES; anything after that
	# keyword is parsed as a test property. The 5 second default is too tight
	# for Windows CI.
	gtest_discover_tests(${name} DISCOVERY_TIMEOUT 600 DISCOVERY_MODE PRE_TEST
		TEST_PREFIX "${prefix}" PROPERTIES LABELS common)
	set_target_properties(${name} PROPERTIES FOLDER "Tests")

	if (CMAKE_COMPILER_IS_GNUCC)
		target_compile_options(${name} PRIVATE "-Wall" "-Werror" "-Wno-unused-variable" "-Wno-unused-result" "-Wno-unused-but-set-variable")
	endif()
endfunction()

fiftyone_add_common_tests(CommonTests "" "")
if (FIFTYONE_COMMON_CXX_BUILD_TESTING_IPI)
	fiftyone_add_common_tests(CommonTestsIpi -ipi "Ipi.")
endif()
