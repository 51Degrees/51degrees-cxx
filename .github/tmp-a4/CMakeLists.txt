# Temporary. Builds the measuring program against one tree, with the same flags
# as the libraries of that tree.
cmake_minimum_required(VERSION 3.24)
project(A4Harness LANGUAGES C CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(FIFTYONE_BUILD_DEVICE_DETECTION OFF CACHE BOOL "" FORCE)
add_subdirectory(${TREE} tree EXCLUDE_FROM_ALL)
add_executable(balanced balanced.c)
target_link_libraries(balanced fiftyone-ip-intelligence-c)
target_include_directories(balanced PRIVATE ${TREE}/src/ipi)
if (MSVC)
	target_compile_options(balanced PRIVATE /D_CRT_SECURE_NO_WARNINGS)
endif()
set_target_properties(balanced PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out)
