# Device Detection libraries, included from the root CMakeLists.txt.

set(DD ${CMAKE_CURRENT_LIST_DIR})
set(HASH ${CMAKE_CURRENT_LIST_DIR}/hash)

FILE(GLOB DDC_SRC ${DD}/*.c)
FILE(GLOB DDC_H ${DD}/*.h)
add_library(fiftyone-device-detection-c ${DDC_SRC} ${DDC_H})
if (MSVC)
	target_link_libraries(fiftyone-device-detection-c fiftyone-common-c)
else()
	target_link_libraries(fiftyone-device-detection-c fiftyone-common-c m)
endif()

FILE(GLOB DDCPP_SRC ${DD}/*.cpp)
FILE(GLOB DDCPP_H ${DD}/*.hpp)
add_library(fiftyone-device-detection-cxx ${DDCPP_SRC} ${DDCPP_H})
target_link_libraries(fiftyone-device-detection-cxx
	fiftyone-device-detection-c
	fiftyone-common-cxx)

set_target_properties(fiftyone-device-detection-c fiftyone-device-detection-cxx
	PROPERTIES FOLDER "Device Detection")

# Hash libraries

FILE(GLOB HASHC_SRC ${HASH}/*.c)
FILE(GLOB HASHC_H ${HASH}/*.h)

add_library(fiftyone-hash-c ${HASHC_SRC} ${HASHC_H})
target_link_libraries(fiftyone-hash-c fiftyone-device-detection-c)
if (MSVC)
	target_compile_options(fiftyone-hash-c PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
else()
	target_compile_options(fiftyone-hash-c PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
	if (MINGW)
		target_compile_options(fiftyone-hash-c PRIVATE "-Wa,-mbig-obj")
	endif()
endif()

FILE(GLOB HASHCPP_SRC ${HASH}/*.cpp)
FILE(GLOB HASHCPP_H ${HASH}/*.hpp)
add_library(fiftyone-hash-cxx ${HASHCPP_SRC} ${HASHCPP_H})
target_link_libraries(fiftyone-hash-cxx fiftyone-hash-c fiftyone-device-detection-cxx)
if (MSVC)
	target_compile_options(fiftyone-hash-cxx PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
else()
	target_compile_options(fiftyone-hash-cxx PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
	# Test coverage instrumentation
	if (TESTCOVERAGE_ENABLED)
		add_library(fiftyone-device-detection-c-cov ${DDC_SRC} ${DDC_H})
		target_link_libraries(fiftyone-device-detection-c-cov fiftyone-common-c m)
		target_compile_options(fiftyone-device-detection-c-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror" "--coverage")

		add_library(fiftyone-device-detection-cxx-cov ${DDCPP_SRC} ${DDCPP_H})
		target_link_libraries(fiftyone-device-detection-cxx-cov fiftyone-device-detection-c-cov fiftyone-common-cxx)
		target_compile_options(fiftyone-device-detection-cxx-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror" "--coverage")

		add_library(fiftyone-hash-c-cov ${HASHC_SRC} ${HASHC_H})
		target_link_libraries(fiftyone-hash-c-cov fiftyone-device-detection-c-cov)
		target_compile_options(fiftyone-hash-c-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror" "--coverage")

		add_library(fiftyone-hash-cxx-cov ${HASHCPP_SRC} ${HASHCPP_H})
		target_link_libraries(fiftyone-hash-cxx-cov fiftyone-hash-c-cov fiftyone-device-detection-cxx-cov)
		target_compile_options(fiftyone-hash-cxx-cov PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror" "--coverage")
	endif()
endif()

set_target_properties(fiftyone-hash-c fiftyone-hash-cxx
	PROPERTIES FOLDER "Device Detection/Hash")

# Shared library

file(WRITE ${CMAKE_BINARY_DIR}/null.cpp "")
add_library(fiftyone-device-detection-complete SHARED ${CMAKE_BINARY_DIR}/null.cpp)
target_link_libraries(fiftyone-device-detection-complete fiftyone-hash-cxx)
if (MSVC)
	target_compile_options(fiftyone-device-detection-complete PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
else()
	target_compile_options(fiftyone-device-detection-complete PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
endif()

set_target_properties(fiftyone-device-detection-complete PROPERTIES PUBLIC_HEADER "${HASH}/hash.h")

install(TARGETS fiftyone-device-detection-complete DESTINATION fiftyone-device-detection-complete
	RUNTIME DESTINATION lib
	ARCHIVE DESTINATION lib/static
	PUBLIC_HEADER DESTINATION include
	# Set this to optional, otherwise it will interfere with builds which
	# include this CMake file.
	OPTIONAL)
