# Install rules, included from the root CMakeLists.txt when FIFTYONE_INSTALL is
# on. They install the libraries of each product built, their headers and a
# CMake package, which another project uses like this.
#
#   find_package(51Degrees CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE 51Degrees::hash-cxx)
#
# The headers include each other by relative path, so they keep the layout of
# src under include/51degrees.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(FIFTYONE_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/51degrees)

# Each product installs the copy of common it links.
set(targets "")
set(headers common)
if (FIFTYONE_BUILD_DEVICE_DETECTION)
	list(APPEND targets
		fiftyone-common-c fiftyone-common-cxx
		fiftyone-device-detection-c fiftyone-device-detection-cxx
		fiftyone-hash-c fiftyone-hash-cxx)
	list(APPEND headers dd)
endif()
if (FIFTYONE_BUILD_IP_INTELLIGENCE)
	list(APPEND targets
		fiftyone-common-c-ipi fiftyone-common-cxx-ipi
		fiftyone-ip-intelligence-c fiftyone-ip-intelligence-cxx)
	list(APPEND headers ipi)
endif()
if (NOT targets)
	return()
endif()

# fiftyone-hash-c is installed as 51Degrees::hash-c, and so on.
foreach (target ${targets})
	string(REGEX REPLACE "^fiftyone-" "" name ${target})
	set_target_properties(${target} PROPERTIES EXPORT_NAME ${name})
endforeach()

# The C++ headers need C++17, so a program that uses them is compiled with it.
foreach (target fiftyone-common-cxx fiftyone-common-cxx-ipi)
	if ("${target}" IN_LIST targets)
		target_compile_features(${target} INTERFACE
			"$<INSTALL_INTERFACE:cxx_std_17>")
	endif()
endforeach()

# MemoryOnly, NoThreading and ExceptionsDisabled set compiler flags, which an
# installed target does not carry, so their definitions are added to it. As in
# this build, NoThreading applies to C only.
set(definitions "")
if (MemoryOnly)
	list(APPEND definitions FIFTYONE_DEGREES_MEMORY_ONLY)
endif()
if (NoThreading)
	list(APPEND definitions
		"$<$<COMPILE_LANGUAGE:C>:FIFTYONE_DEGREES_NO_THREADING>")
endif()
if (ExceptionsDisabled)
	list(APPEND definitions FIFTYONE_DEGREES_EXCEPTIONS_DISABLED)
endif()
foreach (target fiftyone-common-c fiftyone-common-c-ipi)
	if ("${target}" IN_LIST targets)
		foreach (definition IN LISTS definitions)
			target_compile_definitions(${target} INTERFACE
				"$<INSTALL_INTERFACE:${definition}>")
		endforeach()
	endif()
endforeach()

install(TARGETS ${targets} EXPORT 51DegreesTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

foreach (folder ${headers})
	install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../src/${folder}/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/51degrees/${folder}
		FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endforeach()

install(EXPORT 51DegreesTargets
	NAMESPACE 51Degrees::
	FILE 51degrees-targets.cmake
	DESTINATION ${FIFTYONE_INSTALL_CMAKEDIR})

configure_package_config_file(
	${CMAKE_CURRENT_LIST_DIR}/51degrees-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/51degrees-config.cmake
	INSTALL_DESTINATION ${FIFTYONE_INSTALL_CMAKEDIR})
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/51degrees-config-version.cmake
	VERSION ${51Degrees_VERSION}
	COMPATIBILITY SameMajorVersion)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/51degrees-config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/51degrees-config-version.cmake
	DESTINATION ${FIFTYONE_INSTALL_CMAKEDIR})
