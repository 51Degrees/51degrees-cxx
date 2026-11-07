/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is licensed under the European Union Public Licence
 * (EUPL) v.1.2 and is subject to its terms as set out below.
 *
 * If a copy of the EUPL was not distributed with this file, You can obtain
 * one at https://opensource.org/licenses/EUPL-1.2.
 *
 * The 'Compatible Licences' set out in the Appendix to the EUPL (as may be
 * amended by the European Commission) shall be deemed incompatible for
 * the purposes of the Work and the provisions of the compatibility
 * clause in Article 5 of the EUPL shall not apply.
 *
 * If using the Work as, or as part of, a network application, by
 * including the attribution notice(s) required under Article 5 of the EUPL
 * in the end user terms of the application under an appropriate heading,
 * such notice(s) shall fulfill the requirements of that article.
 * ********************************************************************* */

#include "ExampleBase.h"
#include "../../../../src/dd/hash/fiftyone.h"
#include <stdlib.h>
#include <string.h>

#define CONFIG_EQUALS(h) memcmp(&config, &h, sizeof(ConfigHash)) == 0

// Sent with each download so that the downloads these examples make can be
// counted.
#define EXAMPLE_DOWNLOAD_SOURCE "examples-51degrees-cxx"

const char* fiftyoneDegreesExampleGetConfigName(ConfigHash config) {
	if (CONFIG_EQUALS(HashInMemoryConfig)) {
		return "InMemory";
	}
	if (CONFIG_EQUALS(HashHighPerformanceConfig)) {
		return "HighPerformance";
	}
	if (CONFIG_EQUALS(HashLowMemoryConfig)) {
		return "LowMemory";
	}
	if (CONFIG_EQUALS(HashBalancedConfig)) {
		return "Balanced";
	}
	if (CONFIG_EQUALS(HashBalancedTempConfig)) {
		return "BalancedTemp";
	}
	return "Unknown";
}

void fiftyoneDegreesExampleMemCheck(
    fiftyoneDegreesExampleParameters *parameters,
    fiftyoneDegreesExampleRunPtr run) {
// Windows specific memory checking
#if defined(_DEBUG) && defined(_MSC_VER)
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
	
// Use memory tracking for non Windows platforms. can be enabled on Windows
// platform via FORCE_MEMORY_TRACKING
#if (defined(_DEBUG) && !defined(_MSC_VER)) || defined(FORCE_MEMORY_TRACKING)
	fiftyoneDegreesSetUpMemoryTracking();
#endif

	// Call the actual function.
	run(parameters);

// Use memory tracking for non Windows platforms. can be enabled on Windows
// platform via FORCE_MEMORY_TRACKING
#if (defined(_DEBUG) && !defined(_MSC_VER)) || defined(FORCE_MEMORY_TRACKING)
	if (fiftyoneDegreesUnsetMemoryTracking() != 0) {
		printf("ERROR: There is memory leak. All allocated memory should "
			"be freed at the end of this test.\n");
		exit(1);
	}
#endif

// Windows specific memory checking
#if defined(_DEBUG) && defined(_MSC_VER)
	_CrtDumpMemoryLeaks();
#endif
}

// Returns the last path separator in the path, or NULL if there is none.
static char* exampleLastSeparator(char *path) {
	char *last = NULL;
	char *current;
	for (current = path; *current != '\0'; current++) {
		if (*current == '/' || *current == '\\') {
			last = current;
		}
	}
	return last;
}

fiftyoneDegreesStatusCode fiftyoneDegreesExampleGetFilePath(
	const char *dataFolderName,
	const char *fileName,
	char *destination,
	size_t size) {
	char linkName[FILE_MAX_PATH];
	char dataPath[FILE_MAX_PATH];
	char command[FILE_MAX_PATH * 2];
	char *separator;
	int i, written;
	StatusCode status = FileGetPath(
		dataFolderName,
		fileName,
		destination,
		size);
	if (status != FILE_NOT_FOUND) {
		return status;
	}

	// A file the data repository downloads has <file>.link where the file
	// goes, and the data repository is the folder above that one.
	written = Snprintf(linkName, sizeof(linkName), "%s.link", fileName);
	if (written < 0 || (size_t)written >= sizeof(linkName) ||
		FileGetPath(
			dataFolderName,
			linkName,
			dataPath,
			sizeof(dataPath)) != SUCCESS) {
		return status;
	}
	for (i = 0; i < 2; i++) {
		separator = exampleLastSeparator(dataPath);
		if (separator == NULL) {
			return status;
		}
		*separator = '\0';
	}

#ifdef _WIN32
	written = Snprintf(command, sizeof(command),
		"powershell -NoProfile -ExecutionPolicy Bypass -File "
		"\"%s\\get-data-files.ps1\" \"%s\" -Source %s",
		dataPath, fileName, EXAMPLE_DOWNLOAD_SOURCE);
#else
	written = Snprintf(command, sizeof(command),
		"sh \"%s/get-data-files.sh\" -s %s \"%s\"",
		dataPath, EXAMPLE_DOWNLOAD_SOURCE, fileName);
#endif
	if (written < 0 || (size_t)written >= sizeof(command)) {
		return status;
	}

	// Flushed first, as the script writes to the same output.
	printf("%s is missing, so it is being downloaded by running\n%s\n",
		fileName, command);
	fflush(stdout);
	if (system(command) == 0) {
		status = FileGetPath(dataFolderName, fileName, destination, size);
	}
	if (status != SUCCESS) {
		printf("%s could not be downloaded. Run this command to try "
			"again.\n%s\n", fileName, command);
	}
	return status;
}

void fiftyoneDegreesExampleCheckDataFile(
	fiftyoneDegreesDataSetHash *dataset) {
	Item item;
	DataReset(&item.data);

	EXCEPTION_CREATE
	StringGet(dataset->strings, dataset->header.nameOffset, &item, exception);
	EXCEPTION_THROW

	const char* dataTier = STRING(item.data.ptr);

	fiftyoneDegreesDate date = dataset->header.published;
	struct tm gt;
	gt.tm_year = (date.year >= 1900) ? date.year - 1900 : date.year;
	gt.tm_mday = date.day;
	gt.tm_mon = (int)date.month - 1;
	gt.tm_hour = 0;
	gt.tm_min = 0;
	gt.tm_sec = 0;
	gt.tm_isdst = 0;
	gt.tm_wday = 0;
	gt.tm_yday = 0;
	time_t published = mktime(&gt);
	time_t now = time(NULL);

	char timeStr[500] = "";
#if defined( _MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	if (asctime_s(timeStr, 500, (const struct tm*)&gt) != 0) {
#else
	if (asctime_r((const struct tm*)&gt, timeStr) != timeStr) {
#endif
		printf("\033[0;33m");
		printf(("Failed to obtain textual representation of the data file "
			"published date\n"));
		printf("\033[0m");
	}

	printf("Using a %s data file created %s from location %s\n",
		dataTier, timeStr, dataset->b.b.fileName);

#define DATA_FILE_AGE_WARNING 30
	if ((now - published) / (24 * 60 * 60) > DATA_FILE_AGE_WARNING) {
		printf("\033[0;33m");
		printf(("This example is using a data file "
			"that is more than %d days old. "
			"A more recent data file may be needed to "
			"correctly detect the latest devices, browsers, "
			"etc. Get the latest lite data file by running "
			"data/get-data-files.sh -u 51Degrees-LiteV4.1.hash, "
			"or data/get-data-files.ps1 51Degrees-LiteV4.1.hash "
			"-Update on Windows. "
			"Find out about the Enterprise data file, which "
			"includes automatic daily updates, on our pricing "
			"page: https://51degrees.com/pricing?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=data-file-age-warning\n"), DATA_FILE_AGE_WARNING);
		printf("\033[0m");
	}

	if (strncmp(dataTier, "Lite", strlen("Lite")) == 0) {
		printf(("This example is using the \"Lite\" "
			"data file. This is used for illustration, and "
			"has limited accuracy and capabilities. Find "
			"out about the Enterprise data file on our "
			"pricing page: https://51degrees.com/pricing?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=lite-data-file\n"));
	}
	if (!CollectionGetIsMemoryOnly()) {
		COLLECTION_RELEASE(dataset->strings, &item);
	}
}