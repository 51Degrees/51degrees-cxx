# The js-snippet-export tool, included from the root CMakeLists.txt.

add_executable(js-snippet-export ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_link_libraries(js-snippet-export fiftyone-hash-cxx)
if (MSVC)
	target_compile_options(js-snippet-export PRIVATE "/W4" ${_51DEGREES_WX})
	target_link_options(js-snippet-export PRIVATE ${_51DEGREES_WX})
else()
	target_compile_options(js-snippet-export PRIVATE ${COMPILE_OPTION_DEBUG} "-Wall" "-Wextra" ${_51DEGREES_WERROR})
endif()
set_target_properties(js-snippet-export PROPERTIES FOLDER "Tools")

# Optional, so a build of other targets alone can still be installed.
install(TARGETS js-snippet-export RUNTIME DESTINATION bin OPTIONAL)
