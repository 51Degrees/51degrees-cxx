# Defines _51DEGREES_WARNINGS_AS_ERRORS, which makes compiler warnings errors
# when ON. 51Degrees' own builds turn it on, so a warning stops them. A build
# on a customer's machine leaves it off, so a warning from a newer compiler is
# shown and the build carries on.
#
# The option defaults to ON when the environment variable of the same name is
# "true", in any case, and to OFF when it holds anything else or is not set.
# The default is taken when the build folder is first configured, as with any
# option, and -D_51DEGREES_WARNINGS_AS_ERRORS=ON or OFF overrides it.

string(TOLOWER "$ENV{_51DEGREES_WARNINGS_AS_ERRORS}"
	_51DEGREES_WARNINGS_AS_ERRORS_ENV)
if (_51DEGREES_WARNINGS_AS_ERRORS_ENV STREQUAL "true")
	set(_51DEGREES_WARNINGS_AS_ERRORS_DEFAULT ON)
else()
	set(_51DEGREES_WARNINGS_AS_ERRORS_DEFAULT OFF)
endif()
option(_51DEGREES_WARNINGS_AS_ERRORS "Make compiler warnings errors"
	${_51DEGREES_WARNINGS_AS_ERRORS_DEFAULT})
message(STATUS
	"_51DEGREES_WARNINGS_AS_ERRORS=${_51DEGREES_WARNINGS_AS_ERRORS}")

# The targets add _51DEGREES_WERROR to their gcc and clang options and
# _51DEGREES_WX to their MSVC options. Each is empty when the option is off,
# which also leaves CMAKE_COMPILE_WARNING_AS_ERROR as the caller set it.
if (_51DEGREES_WARNINGS_AS_ERRORS)
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
	set(_51DEGREES_WERROR "-Werror")
	set(_51DEGREES_WX "/WX")
else()
	set(_51DEGREES_WERROR "")
	set(_51DEGREES_WX "")
endif()
