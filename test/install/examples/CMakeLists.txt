# Builds the examples against an installed copy of the libraries, as a
# customer who has the package would. ci/check-install.ps1 gives it a copy of
# the examples folder with the installed headers beside it in a folder named
# src, because the examples include the headers by path from there. Like the
# project in the folder above, it sets no language standard.

cmake_minimum_required(VERSION 3.24)
project(InstalledExamples LANGUAGES C CXX)

find_package(51Degrees CONFIG REQUIRED)

set(EXAMPLES "" CACHE PATH "The copy of the examples folder")
if (NOT IS_DIRECTORY "${EXAMPLES}")
	message(FATAL_ERROR "EXAMPLES is not a folder: '${EXAMPLES}'")
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
				target_compile_definitions(${name}${suffix}
					PRIVATE _CRT_SECURE_NO_WARNINGS)
			endif()
			message(STATUS "Found example '${name}${suffix}'")
		endif()
	endforeach()
endfunction()

if (TARGET 51Degrees::hash-c)
	set(DD ${EXAMPLES}/dd)
	add_examples(${DD}/C/Hash c HashC 51Degrees::hash-c
		${DD}/C/Hash/ExampleBase.c)
	add_examples(${DD}/CPP/Hash cpp HashCPP 51Degrees::hash-cxx
		${DD}/C/Hash/ExampleBase.c ${DD}/CPP/Hash/ExampleBase.cpp)
endif()

if (TARGET 51Degrees::ip-intelligence-c)
	set(IPI ${EXAMPLES}/ipi)
	add_examples(${IPI}/C/IpIntelligence c C 51Degrees::ip-intelligence-c
		${IPI}/Base/ExampleBase.c)
	add_examples(${IPI}/CPP/IpIntelligence cpp CPP
		51Degrees::ip-intelligence-cxx
		${IPI}/Base/ExampleBase.c ${IPI}/CPP/IpIntelligence/ExampleBase.cpp)
endif()
