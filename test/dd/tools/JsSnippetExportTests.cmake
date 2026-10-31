# Runs the real js-snippet-export tool and checks what it writes. CTest passes
# TOOL, the tool's path, DATA, the Lite data file, and WORK, a folder to write
# in. A failed check is reported and the checks after it still run.
cmake_minimum_required(VERSION 3.24)

# Runs the tool, which must exit with the code given and report success only
# when that code is 0.
function(expectExit code)
	execute_process(COMMAND "${TOOL}" ${ARGN}
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
	string(FIND "${output}" "Export complete!" complete)
	if (NOT result STREQUAL code)
		message(SEND_ERROR "Exit '${result}', expected ${code}: ${ARGN}\n${output}")
	elseif (code EQUAL 0 AND complete EQUAL -1)
		message(SEND_ERROR "No success message: ${ARGN}\n${output}")
	elseif (NOT code EQUAL 0 AND NOT complete EQUAL -1)
		message(SEND_ERROR "Success message on failure: ${ARGN}\n${output}")
	endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# A path the manifest has to escape, with backslashes on Windows, and a quote
# and a backslash elsewhere.
if (CMAKE_HOST_WIN32)
	file(TO_NATIVE_PATH "${DATA}" data)
else()
	set(data "${WORK}/da\"ta\\.hash")
	file(CREATE_LINK "${DATA}" "${data}" SYMBOLIC)
endif()

# Bad arguments and paths must end in a plain failure rather than an abort.
expectExit(1)
expectExit(1 -d)
expectExit(1 -d "${WORK}/missing.hash" -o "${WORK}/unused")
expectExit(1 -d "/root/nope/missing.hash" -o "${WORK}/unused")
file(TOUCH "${WORK}/afile")
expectExit(1 -d "${data}" -o "${WORK}/afile/out")
if (CMAKE_HOST_UNIX)
	# /root cannot be read or created unless running as root.
	execute_process(COMMAND id -u OUTPUT_VARIABLE uid
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if (NOT uid EQUAL 0)
		expectExit(1 -d "${data}" -o "/root/nope/out")
	endif()
endif()

# A write that fails must fail the export. Every write to this device fails.
if (EXISTS /dev/full)
	file(MAKE_DIRECTORY "${WORK}/full")
	file(CREATE_LINK /dev/full "${WORK}/full/manifest.json" SYMBOLIC)
	expectExit(1 -d "${data}" -o "${WORK}/full")
endif()

# A real export, to a new folder named with a trailing separator.
set(out "${WORK}/nested/out/")
expectExit(0 -d "${data}" -o "${out}")
file(GLOB files "${out}*.js")
list(LENGTH files fileCount)
if (fileCount EQUAL 0)
	message(FATAL_ERROR "No snippets in ${out}")
endif()

# Each snippet must be the exact bytes held in the data file.
file(READ "${DATA}" dataBytes HEX)
foreach (file ${files})
	file(READ "${file}" bytes HEX)
	string(FIND "${dataBytes}" "${bytes}" position)
	if (bytes STREQUAL "" OR position EQUAL -1)
		message(SEND_ERROR "Not the bytes in the data file: ${file}")
	endif()
endforeach()

# string(JSON) stops with an error if the manifest is not valid JSON.
file(READ "${out}manifest.json" manifest)
string(JSON dataFile GET "${manifest}" dataFile)
if (NOT dataFile STREQUAL data)
	message(SEND_ERROR "dataFile is '${dataFile}', expected '${data}'")
endif()
string(JSON generated GET "${manifest}" generated)
if (NOT generated MATCHES "^[0-9-]+T[0-9:]+Z$")
	message(SEND_ERROR "generated is '${generated}'")
endif()
string(JSON propertyCount GET "${manifest}" jsPropertyCount)
string(JSON snippetCount GET "${manifest}" snippetCount)
string(JSON listed LENGTH "${manifest}" snippets)
if (propertyCount LESS 1 OR NOT snippetCount EQUAL fileCount OR
	NOT listed EQUAL fileCount)
	message(SEND_ERROR "Properties ${propertyCount}, count ${snippetCount}, "
		"listed ${listed}, files ${fileCount}")
endif()
math(EXPR last "${listed} - 1")
foreach (i RANGE ${last})
	string(JSON name GET "${manifest}" snippets ${i} file)
	string(JSON property GET "${manifest}" snippets ${i} property)
	string(JSON index GET "${manifest}" snippets ${i} index)
	string(TOLOWER "${property}_${index}.js" expected)
	if (NOT name STREQUAL expected OR NOT EXISTS "${out}${name}")
		message(SEND_ERROR "Listed '${name}', expected the file '${expected}'")
	endif()
endforeach()
