# The js-snippet-export tool, included from the root CMakeLists.txt.

add_executable(js-snippet-export ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_link_libraries(js-snippet-export fiftyone-hash-cxx)
if (MSVC)
	target_compile_options(js-snippet-export PRIVATE "/D_CRT_SECURE_NO_WARNINGS" "/W4" "/WX")
else()
	target_compile_options(js-snippet-export PRIVATE ${COMPILE_OPTION_DEBUG} "-Werror")
endif()
set_target_properties(js-snippet-export PROPERTIES FOLDER "Tools")

# Optional, so a build of other targets alone can still be installed.
install(TARGETS js-snippet-export RUNTIME DESTINATION bin OPTIONAL)
