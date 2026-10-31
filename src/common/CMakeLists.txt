# Common libraries, included from the root CMakeLists.txt.
#
# Device Detection and IP Intelligence each need common built differently.
# IP Intelligence data files can exceed 4 GB and hold a reduced profile
# record, so its copy is built with 64 bit file offsets and the reduced
# layout. The switches change structure sizes, so the two copies cannot be
# shared and each product links its own. FIFTYONE_DEGREES_IPI_SYMBOLS gives the
# IP Intelligence copy its own linked names (see ipiSymbols.h), so one program
# can link both.

FILE(GLOB COMC_SRC ${CMAKE_CURRENT_LIST_DIR}/*.c)
FILE(GLOB COMC_H ${CMAKE_CURRENT_LIST_DIR}/*.h)
FILE(GLOB COMCPP_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
FILE(GLOB COMCPP_H ${CMAKE_CURRENT_LIST_DIR}/*.hpp)

# Adds fiftyone-common-c<suffix> and fiftyone-common-cxx<suffix>, plus copies
# ending -cov that are built for coverage when it is enabled. The remaining
# arguments are compile definitions. They are public, so every target that
# links the libraries is compiled with them too.
function(fiftyone_add_common suffix)
	set(variants "")
	if (TESTCOVERAGE_ENABLED)
		set(variants "-cov")
	endif()
	foreach (variant "" ${variants})
		set(c fiftyone-common-c${suffix}${variant})
		set(cxx fiftyone-common-cxx${suffix}${variant})
		add_library(${c} ${COMC_SRC} ${COMC_H})
		target_link_libraries(${c} ${CMAKE_THREAD_LIBS_INIT} ${GCCLIBATOMIC_LIBRARY})
		target_compile_definitions(${c} PUBLIC ${ARGN})
		add_library(${cxx} ${COMCPP_SRC} ${COMCPP_H})
		target_link_libraries(${cxx} ${c})
		set_target_properties(${c} ${cxx} PROPERTIES FOLDER "Common")
		if (MSVC)
			target_compile_options(${c} PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
			target_compile_options(${cxx} PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX" "/EHsc")
		else()
			target_compile_options(${c} PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror" "-Wno-atomic-alignment")
			target_compile_options(${cxx} PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
			if (variant STREQUAL "-cov")
				target_compile_options(${c} PRIVATE "--coverage")
				target_compile_options(${cxx} PRIVATE "--coverage")
			endif()
		endif()
	endforeach()
endfunction()

set(FIFTYONE_COMMON_DEFINITIONS "")
if (LargeDataFileSupport)
	list(APPEND FIFTYONE_COMMON_DEFINITIONS
		FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT _FILE_OFFSET_BITS=64)
endif()
if (ReducedFile)
	list(APPEND FIFTYONE_COMMON_DEFINITIONS FIFTYONE_DEGREES_REDUCED_FILE)
endif()
fiftyone_add_common("" ${FIFTYONE_COMMON_DEFINITIONS})

if (FIFTYONE_BUILD_IP_INTELLIGENCE OR FIFTYONE_COMMON_CXX_BUILD_TESTING_IPI)
	set(FIFTYONE_COMMON_IPI_DEFINITIONS
		FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
		_FILE_OFFSET_BITS=64
		FIFTYONE_DEGREES_REDUCED_FILE)
	if (FIFTYONE_IPI_SYMBOLS)
		list(APPEND FIFTYONE_COMMON_IPI_DEFINITIONS FIFTYONE_DEGREES_IPI_SYMBOLS)
	endif()
	fiftyone_add_common(-ipi ${FIFTYONE_COMMON_IPI_DEFINITIONS})
endif()
