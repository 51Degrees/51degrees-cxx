# One program that links both products, included from the root CMakeLists.txt
# when both are built. The definitions on the IP Intelligence libraries change
# structure sizes, so each product's code compiles apart as an object library
# that sees only that product's libraries. main.c includes neither product.

add_library(BothProductsDd OBJECT ${CMAKE_CURRENT_LIST_DIR}/dd.c)
target_link_libraries(BothProductsDd PRIVATE fiftyone-hash-c)

add_library(BothProductsIpi OBJECT ${CMAKE_CURRENT_LIST_DIR}/ipi.c)
target_link_libraries(BothProductsIpi PRIVATE fiftyone-ip-intelligence-c)

add_executable(BothProducts
	${CMAKE_CURRENT_LIST_DIR}/main.c
	${CMAKE_CURRENT_LIST_DIR}/both.h
	$<TARGET_OBJECTS:BothProductsDd>
	$<TARGET_OBJECTS:BothProductsIpi>)
target_link_libraries(BothProducts fiftyone-hash-c fiftyone-ip-intelligence-c)

foreach (target BothProductsDd BothProductsIpi BothProducts)
	if (MSVC)
		target_compile_options(${target} PRIVATE "/W4" "/WX")
	else()
		target_compile_options(${target} PRIVATE "-Wall" "-Wextra" "-Werror")
	endif()
endforeach()

set_target_properties(BothProductsDd BothProductsIpi BothProducts
	PROPERTIES FOLDER "Tests")

# A return code of 77 means the data files are missing, so the lookups did not
# run. The ipi label runs it once in each job that builds both products.
add_test(NAME BothProducts COMMAND BothProducts)
set_tests_properties(BothProducts PROPERTIES
	LABELS ipi
	SKIP_RETURN_CODE 77)

if (_51DEGREES_CXX_BOTH_PRODUCTS)
	add_library(BothProductsCxxDd OBJECT ${CMAKE_CURRENT_LIST_DIR}/dd.cpp)
	target_link_libraries(BothProductsCxxDd PRIVATE fiftyone-hash-cxx)

	add_library(BothProductsCxxIpi OBJECT ${CMAKE_CURRENT_LIST_DIR}/ipi.cpp)
	target_link_libraries(BothProductsCxxIpi PRIVATE fiftyone-ip-intelligence-cxx)

	add_executable(BothProductsCxx
		${CMAKE_CURRENT_LIST_DIR}/main.cpp
		${CMAKE_CURRENT_LIST_DIR}/both.h
		$<TARGET_OBJECTS:BothProductsCxxDd>
		$<TARGET_OBJECTS:BothProductsCxxIpi>)
	target_link_libraries(BothProductsCxx
		fiftyone-hash-cxx
		fiftyone-ip-intelligence-cxx)

	foreach (target BothProductsCxxDd BothProductsCxxIpi BothProductsCxx)
		if (MSVC)
			target_compile_options(${target} PRIVATE "/W4" "/WX" "/EHsc")
		else()
			target_compile_options(${target} PRIVATE "-Wall" "-Wextra" "-Werror")
		endif()
	endforeach()

	set_target_properties(BothProductsCxxDd BothProductsCxxIpi BothProductsCxx
		PROPERTIES FOLDER "Tests")

	add_test(NAME BothProductsCxx COMMAND BothProductsCxx)
	set_tests_properties(BothProductsCxx PROPERTIES
		LABELS ipi
		SKIP_RETURN_CODE 77)
endif()
