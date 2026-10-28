# Device Detection examples, included from the root CMakeLists.txt.

set(DD_EXAMPLES ${CMAKE_CURRENT_LIST_DIR})

MESSAGE("-- Looking for examples...")
foreach (api "Hash")
	foreach (langext c cpp)
		string( TOUPPER ${langext} upperlangext)
		file(GLOB files ${DD_EXAMPLES}/${upperlangext}/${api}/*.${langext})
		foreach(file ${files})
			string( REPLACE ".${langext}" "" name ${file})
			string( REPLACE "${DD_EXAMPLES}/${upperlangext}/${api}/" "" name ${name})
			string( APPEND name ${api} ${upperlangext})
			string( TOLOWER ${api} lowerapi)

			if (NOT ${name} MATCHES ".*ExampleBase.*")
				if("${langext}" STREQUAL "cpp")
					MESSAGE("-- Found C++ example '${name}'")
					add_executable(${name} ${file}
					${DD_EXAMPLES}/C/${api}/ExampleBase.c
					${DD_EXAMPLES}/C/${api}/ExampleBase.h
					${DD_EXAMPLES}/${upperlangext}/${api}/ExampleBase.cpp
					${DD_EXAMPLES}/${upperlangext}/${api}/ExampleBase.hpp)
					if (MSVC)
						target_link_libraries(${name} fiftyone-${lowerapi}-cxx)
					else()
						if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
							target_include_directories(${name} PRIVATE "/usr/local/include")
							target_link_directories(${name} PRIVATE "/usr/local/lib")
						endif()
						target_link_libraries(${name} fiftyone-${lowerapi}-cxx)
					endif()
				elseif("${langext}" STREQUAL "c")
					MESSAGE("-- Found C example '${name}'")
					add_executable(${name} ${file}
					${DD_EXAMPLES}/${upperlangext}/${api}/ExampleBase.c
					${DD_EXAMPLES}/${upperlangext}/${api}/ExampleBase.h)
					if (MSVC)
						target_link_libraries(${name} fiftyone-${lowerapi}-c)
					else()
						if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
							target_include_directories(${name} PRIVATE "/usr/local/include")
							target_link_directories(${name} PRIVATE "/usr/local/lib")
						endif()
						target_link_libraries(${name} fiftyone-${lowerapi}-c)
					endif()
				endif()
				set_target_properties(${name}
					PROPERTIES FOLDER
					"Examples/Device Detection/${api}/${upperlangext}")
				if (MSVC)
					target_compile_options(${name} PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
					target_link_options(${name} PRIVATE "/WX")
				else()
					target_compile_options(${name} PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
				endif()
			endif()
		endforeach()
	endforeach()
endforeach()
