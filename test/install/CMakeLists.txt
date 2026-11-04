# A project that uses an installed copy of the libraries, as a customer's
# would. ci/check-install.ps1 builds it against an install and runs it. It sets
# no language standard, so the targets have to bring the one they need.

cmake_minimum_required(VERSION 3.24)
project(InstallCheck LANGUAGES C CXX)

# When given, the version the installed package has to report.
set(EXPECTED_VERSION "" CACHE STRING "Version the package must report")
if (EXPECTED_VERSION)
	find_package(51Degrees ${EXPECTED_VERSION} EXACT CONFIG REQUIRED)
else()
	find_package(51Degrees CONFIG REQUIRED)
endif()

if (TARGET 51Degrees::hash-c)
	add_executable(dd-c dd.c)
	target_link_libraries(dd-c PRIVATE 51Degrees::hash-c)
	add_executable(dd-cxx dd.cpp)
	target_link_libraries(dd-cxx PRIVATE 51Degrees::hash-cxx)
endif()

if (TARGET 51Degrees::ip-intelligence-c)
	add_executable(ipi-c ipi.c)
	target_link_libraries(ipi-c PRIVATE 51Degrees::ip-intelligence-c)
	add_executable(ipi-cxx ipi.cpp)
	target_link_libraries(ipi-cxx PRIVATE 51Degrees::ip-intelligence-cxx)
endif()
