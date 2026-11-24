# IP Intelligence libraries, included from the root CMakeLists.txt. They link
# the copy of common built for IP Intelligence (see src/common/CMakeLists.txt).

set(IPI ${CMAKE_CURRENT_LIST_DIR})
set(IPI_GRAPH ${CMAKE_CURRENT_LIST_DIR}/graph)

FILE(GLOB IPIC_SRC ${IPI}/*.c ${IPI_GRAPH}/*.c)
FILE(GLOB IPIC_H ${IPI}/*.h ${IPI_GRAPH}/*.h)
add_library(fiftyone-ip-intelligence-c ${IPIC_SRC} ${IPIC_H})
if (MSVC)
	target_link_libraries(fiftyone-ip-intelligence-c fiftyone-common-c-ipi)
	target_compile_options(fiftyone-ip-intelligence-c PRIVATE "/W4" "/WX")
else()
	target_link_libraries(fiftyone-ip-intelligence-c fiftyone-common-c-ipi m)
	target_compile_options(fiftyone-ip-intelligence-c PRIVATE ${COMPILE_OPTION_DEBUG} "-Wall" "-Wextra" "-Werror")
endif()

FILE(GLOB IPICPP_SRC ${IPI}/*.cpp)
FILE(GLOB IPICPP_H ${IPI}/*.hpp)
add_library(fiftyone-ip-intelligence-cxx ${IPICPP_SRC} ${IPICPP_H})
target_link_libraries(fiftyone-ip-intelligence-cxx
	fiftyone-ip-intelligence-c
	fiftyone-common-cxx-ipi)
if (MSVC)
	target_compile_options(fiftyone-ip-intelligence-cxx PRIVATE "/W4" "/WX")
else()
	target_compile_options(fiftyone-ip-intelligence-cxx PRIVATE ${COMPILE_OPTION_DEBUG} "-Wall" "-Wextra" "-Werror")
endif()

if (TESTCOVERAGE_ENABLED)
	message("-- fiftyone-ip-intelligence-c(xx)-cov adding targets with code coverage")
	add_library(fiftyone-ip-intelligence-c-cov ${IPIC_SRC} ${IPIC_H})
	if (MSVC)
		target_link_libraries(fiftyone-ip-intelligence-c-cov fiftyone-common-c-ipi-cov)
	else()
		target_link_libraries(fiftyone-ip-intelligence-c-cov fiftyone-common-c-ipi-cov m)
		target_compile_options(fiftyone-ip-intelligence-c-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Wall" "-Wextra" "-Werror")
	endif()
	target_compile_options(fiftyone-ip-intelligence-c-cov PRIVATE "--coverage")

	add_library(fiftyone-ip-intelligence-cxx-cov ${IPICPP_SRC} ${IPICPP_H})
	target_link_libraries(fiftyone-ip-intelligence-cxx-cov
		fiftyone-ip-intelligence-c-cov
		fiftyone-common-cxx-ipi-cov)
	if (NOT MSVC)
		target_compile_options(fiftyone-ip-intelligence-cxx-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Wall" "-Wextra" "-Werror")
	endif()
	target_compile_options(fiftyone-ip-intelligence-cxx-cov PRIVATE "--coverage")

	set_target_properties(fiftyone-ip-intelligence-c-cov fiftyone-ip-intelligence-cxx-cov
		PROPERTIES FOLDER "IP Intelligence")
endif()

set_target_properties(fiftyone-ip-intelligence-c fiftyone-ip-intelligence-cxx
	PROPERTIES FOLDER "IP Intelligence")

# Shared library

file(WRITE ${CMAKE_BINARY_DIR}/null.cpp "")
add_library(fiftyone-ip-intelligence-complete SHARED ${CMAKE_BINARY_DIR}/null.cpp)
target_link_libraries(fiftyone-ip-intelligence-complete fiftyone-ip-intelligence-cxx)
