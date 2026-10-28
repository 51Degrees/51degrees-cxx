# IP Intelligence examples, included from the root CMakeLists.txt.

set(IPI_EXAMPLES ${CMAKE_CURRENT_LIST_DIR})
set(EXAMPLES_BASE_SRC ${IPI_EXAMPLES}/Base/ExampleBase.c)
set(EXAMPLES_BASE_H ${IPI_EXAMPLES}/Base/ExampleBase.h)
set(EXAMPLES_FOLDER IpIntelligence)

MESSAGE("-- Looking for examples...")
foreach (api "IP-Intelligence")
	foreach (langext c cpp)
		string( TOUPPER ${langext} upperlangext)
		file(GLOB files ${IPI_EXAMPLES}/${upperlangext}/${EXAMPLES_FOLDER}/*.${langext})
		foreach(file ${files})
			string( REPLACE ".${langext}" "" name ${file})
			string( REPLACE "${IPI_EXAMPLES}/${upperlangext}/${EXAMPLES_FOLDER}/" "" name ${name})
			string( APPEND name ${upperlangext})
			string( TOLOWER ${api} lowerapi)

			if (NOT ${name} MATCHES ".*ExampleBase.*")
				if("${langext}" STREQUAL "cpp")
					MESSAGE("-- Found C++ example '${name}'")
					add_executable(${name} ${file}
					${IPI_EXAMPLES}/${upperlangext}/${EXAMPLES_FOLDER}/ExampleBase.cpp
					${IPI_EXAMPLES}/${upperlangext}/${EXAMPLES_FOLDER}/ExampleBase.hpp
					${EXAMPLES_BASE_SRC} ${EXAMPLES_BASE_H})
					target_link_libraries(${name} fiftyone-${lowerapi}-cxx)
				elseif("${langext}" STREQUAL "c")
					MESSAGE("-- Found C example '${name}'")
					add_executable(${name} ${file}
					${EXAMPLES_BASE_SRC} ${EXAMPLES_BASE_H})
					target_link_libraries(${name} fiftyone-${lowerapi}-c)
				endif()
				set_target_properties(${name}
					PROPERTIES FOLDER
					"Examples/IP Intelligence/${upperlangext}")
				if (MSVC)
					target_compile_options(${name} PRIVATE "/D_CRT_SECURE_NO_WARNINGS")
				endif()
			endif()
		endforeach()
	endforeach()
endforeach()
