# Builds the examples against an installed copy of the libraries, as a
# customer who has the package would. ci/check-install.ps1 gives it a copy of
# the examples folder with the installed headers beside it in a folder named
# src, because the examples include the headers by path from there. Like the
# project in the folder above, it sets no language standard.

cmake_minimum_required(VERSION 3.24)
project(InstalledExamples LANGUAGES C CXX)

find_package(51Degrees CONFIG REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/warnings-as-errors.cmake)

set(_51DEGREES_EXAMPLES "" CACHE PATH "The copy of the examples folder")
if (NOT IS_DIRECTORY "${_51DEGREES_EXAMPLES}")
	message(FATAL_ERROR "_51DEGREES_EXAMPLES is not a folder: '${_51DEGREES_EXAMPLES}'")
endif()

# Adds a program for each example in a folder, named as the build of this
# repository names it. The sources after the library are built into each.
function(add_examples folder extension suffix library)
	file(GLOB files ${folder}/*.${extension})
	foreach (file ${files})
		get_filename_component(name ${file} NAME_WE)
		if (NOT name STREQUAL "ExampleBase")
			add_executable(${name}${suffix} ${file} ${ARGN})
			target_link_libraries(${name}${suffix} PRIVATE ${library})
			if (MSVC)
				target_compile_options(${name}${suffix} PRIVATE /W4 ${_51DEGREES_WX})
			else()
				target_compile_options(${name}${suffix}
					PRIVATE -Wall -Wextra ${_51DEGREES_WERROR})
			endif()
			message(STATUS "Found example '${name}${suffix}'")
		endif()
	endforeach()
endfunction()

if (TARGET 51Degrees::hash-c)
	set(DD ${_51DEGREES_EXAMPLES}/dd)
	add_examples(${DD}/C/Hash c HashC 51Degrees::hash-c
		${DD}/C/Hash/ExampleBase.c)
	add_examples(${DD}/CPP/Hash cpp HashCPP 51Degrees::hash-cxx
		${DD}/C/Hash/ExampleBase.c ${DD}/CPP/Hash/ExampleBase.cpp)
endif()

if (TARGET 51Degrees::ip-intelligence-c)
	set(IPI ${_51DEGREES_EXAMPLES}/ipi)
	add_examples(${IPI}/C/IpIntelligence c C 51Degrees::ip-intelligence-c
		${IPI}/Base/ExampleBase.c)
	add_examples(${IPI}/CPP/IpIntelligence cpp CPP
		51Degrees::ip-intelligence-cxx
		${IPI}/Base/ExampleBase.c ${IPI}/CPP/IpIntelligence/ExampleBase.cpp)
endif()
