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
// counted. It is left out inside GitHub Actions.
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

fiftyoneDegreesStatusCode fiftyoneDegreesExampleCopyPath(
	const char *source,
	char *destination,
	size_t size) {
	size_t length = strlen(source);
	if (length >= size) {
		fprintf(stderr, "The path '%s' is longer than the %d characters "
			"allowed.\n", source, (int)size - 1);
		return FILE_PATH_TOO_LONG;
	}
	memcpy(destination, source, length + 1);
	return SUCCESS;
}

FILE* fiftyoneDegreesExampleFileOpenWrite(const char *fileName) {
#ifdef _MSC_VER
	FILE *file = NULL;
	if (fopen_s(&file, fileName, "w") != 0) {
		return NULL;
	}
	return file;
#else
	return fopen(fileName, "w");
#endif
}

/**
 * Copies the value of an environment variable to the destination, cut to fit
 * when it is longer.
 * @param name of the environment variable
 * @param destination memory to copy the value to
 * @param size of the destination in bytes
 * @return true when the variable is set
 */
static bool exampleGetEnvironmentVariable(
	const char *name,
	char *destination,
	size_t size) {
	bool found = false;
	size_t length;
#ifdef _MSC_VER
	char *value = NULL;
	size_t valueSize = 0;
	if (_dupenv_s(&value, &valueSize, name) != 0) {
		value = NULL;
	}
#else
	const char *value = getenv(name);
#endif
	if (value != NULL && size > 0) {
		length = strlen(value);
		if (length >= size) {
			length = size - 1;
		}
		memcpy(destination, value, length);
		destination[length] = '\0';
		found = true;
	}
#ifdef _MSC_VER
	free(value);
#endif
	return found;
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
	char actions[16];
	char *separator;
	int i, written;
	bool inActions;
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

	// Inside GitHub Actions the data repository's script names the build,
	// so that test runs are not counted as people using the examples.
	inActions = exampleGetEnvironmentVariable(
		"GITHUB_ACTIONS",
		actions,
		sizeof(actions)) &&
		strcmp(actions, "true") == 0;
#ifdef _WIN32
	// PowerShell 7 when it is on the path, otherwise Windows PowerShell,
	// which, started from PowerShell 7, inherits its module path and then
	// cannot load its own commands.
	written = Snprintf(command, sizeof(command),
		"%s -NoProfile -ExecutionPolicy Bypass -File "
		"\"%s\\get-data-files.ps1\" \"%s\"%s%s",
		system("where pwsh >NUL 2>NUL") == 0 ?
			"pwsh" : "set \"PSModulePath=\" && powershell",
		dataPath, fileName,
		inActions ? "" : " -Source ",
		inActions ? "" : EXAMPLE_DOWNLOAD_SOURCE);
#else
	written = Snprintf(command, sizeof(command),
		"sh \"%s/get-data-files.sh\"%s%s \"%s\"",
		dataPath,
		inActions ? "" : " -s ",
		inActions ? "" : EXAMPLE_DOWNLOAD_SOURCE,
		fileName);
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

bool fiftyoneDegreesExampleHasProperty(
	fiftyoneDegreesDataSetHash *dataset,
	const char *propertyName) {
	Item item;
	DataReset(&item.data);
	EXCEPTION_CREATE
	const Property *property = PropertyGetByName(
		dataset->properties,
		dataset->strings,
		propertyName,
		&item,
		exception);
	if (property == NULL) {
		return false;
	}
	// A property that was returned holds its item, even when the lookup set
	// the exception, so the item is released before the result is given.
	COLLECTION_RELEASE(item.collection, &item);
	return !EXCEPTION_FAILED;
}

void fiftyoneDegreesExampleOutputEnterpriseValue(
	fiftyoneDegreesResultsHash *results,
	const char *propertyName) {
	char buffer[500];
	buffer[0] = '\0';
	EXCEPTION_CREATE
	if (fiftyoneDegreesExampleHasProperty(
		(DataSetHash*)results->b.b.dataSet,
		propertyName) == false) {
		printf("   %s: %s\n", propertyName,
			FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE);
		return;
	}
	ResultsHashGetValuesString(
		results,
		propertyName,
		buffer,
		sizeof(buffer),
		(char*)",",
		exception);
	EXCEPTION_THROW
	printf("   %s: %s\n", propertyName, buffer);
}

/**
 * Writes a link with the example's own file as its utm_content in place of
 * this file's, so that an enquiry can be traced to the example.
 */
static void writeLink(FILE *output, const char *link, const char *example) {
	static const char content[] = "utm_content=";
	const char *start = strstr(link, content);
	const char *rest = start == NULL ? NULL : strchr(start, '&');
	if (example == NULL || rest == NULL) {
		fputs(link, output);
		return;
	}
	start += sizeof(content) - 1;
	fprintf(output, "%.*s%s%s", (int)(start - link), link, example, rest);
}

void fiftyoneDegreesExampleOfferEnterprise(
	fiftyoneDegreesResourceManager *manager,
	const char *example) {
	DataSetHash *dataset = DataSetHashGet(manager);
	fiftyoneDegreesExampleCheckDataFile(dataset, example);
	DataSetHashRelease(dataset);
}

void fiftyoneDegreesExampleReportEnterpriseProperties(
	FILE *output,
	fiftyoneDegreesDataSetHash *dataset,
	const char *propertyNames) {
	char name[100];
	const char *start = propertyNames == NULL || *propertyNames == '\0' ?
		FIFTYONE_DEGREES_EXAMPLE_ENTERPRISE_PROPERTIES : propertyNames;
	while (start != NULL) {
		const char *end = strchr(start, ',');
		size_t length = end == NULL ? strlen(start) : (size_t)(end - start);
		while (length > 0 && *start == ' ') {
			start++;
			length--;
		}
		while (length > 0 && start[length - 1] == ' ') {
			length--;
		}
		if (length > 0 && length < sizeof(name)) {
			memcpy(name, start, length);
			name[length] = '\0';
			if (fiftyoneDegreesExampleHasProperty(dataset, name) == false) {
				fprintf(output, "%s %s.\n", name,
					FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE);
			}
		}
		start = end == NULL ? NULL : end + 1;
	}
}

void fiftyoneDegreesExampleOfferEnterpriseProperties(
	fiftyoneDegreesResourceManager *manager,
	const char *propertyNames,
	const char *example) {
	DataSetHash *dataset = DataSetHashGet(manager);
	fiftyoneDegreesExampleReportEnterpriseProperties(
		stdout,
		dataset,
		propertyNames);
	fiftyoneDegreesExampleCheckDataFile(dataset, example);
	DataSetHashRelease(dataset);
}

void fiftyoneDegreesExampleCheckDataFile(
	fiftyoneDegreesDataSetHash *dataset,
	const char *example) {
	Item item;
	DataReset(&item.data);

	EXCEPTION_CREATE
	StringGet(dataset->strings, dataset->header.nameOffset, &item, exception);
	EXCEPTION_THROW

	const char* dataTier = STRING(item.data.ptr);

	// The header holds whatever bytes the file was written with, so the date
	// is only turned into a time when it is a real calendar date.
	fiftyoneDegreesDate date = dataset->header.published;
	const bool hasDate = DateIsValid(&date);
	time_t published = 0;
	time_t now = time(NULL);
	char timeStr[500] = "";
	if (hasDate) {
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
		published = mktime(&gt);
#if defined( _MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
		if (asctime_s(timeStr, 500, (const struct tm*)&gt) != 0) {
#else
		if (asctime_r((const struct tm*)&gt, timeStr) != timeStr) {
#endif
			printf("\033[0;33m");
			printf(("Failed to obtain textual representation of the data "
				"file published date\n"));
			printf("\033[0m");
		}
		printf("Using a %s data file created %s from location %s\n",
			dataTier, timeStr, dataset->b.b.fileName);
	}
	else {
		printf("Using a %s data file with no valid published date from "
			"location %s\n", dataTier, dataset->b.b.fileName);
	}

#define DATA_FILE_AGE_WARNING 30
	if (hasDate &&
		(now - published) / (24 * 60 * 60) > DATA_FILE_AGE_WARNING) {
		printf("\033[0;33m");
		printf(("This example is using a data file "
			"that is more than %d days old. "
			"A more recent data file may be needed to "
			"correctly detect the latest devices, browsers, "
			"etc. Get the latest lite data file by running "
			"data/get-data-files.sh -u 51Degrees-LiteV4.1.hash, "
			"or data/get-data-files.ps1 51Degrees-LiteV4.1.hash "
			"-Update on Windows. "
			"Contact us about the Enterprise data file, which "
			"includes automatic daily updates, at "), DATA_FILE_AGE_WARNING);
		writeLink(stdout, "https://51degrees.com/contact-us?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=data-file-age-warning", example);
		printf("\n\033[0m");
	}

	if (strncmp(dataTier, "Lite", strlen("Lite")) == 0) {
		printf("This example is using the free Lite data file. The "
			"Enterprise data file adds many more properties, such as "
			"HardwareVendor, HardwareName and DeviceType. Contact us for "
			"a copy at ");
		writeLink(stdout, "https://51degrees.com/contact-us?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=lite-data-file", example);
		printf("\n");
	}
	if (!CollectionGetIsMemoryOnly()) {
		COLLECTION_RELEASE(dataset->strings, &item);
	}
}

void fiftyoneDegreesExampleFindOutMore(FILE *output, const char *example) {
	fprintf(output, "\nFind out more\n-------------\nDevice Detection: ");
	writeLink(output, "https://51degrees.com/device-detection?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=find-out-more", example);
	fprintf(output, "\nDocumentation: ");
	writeLink(output, "https://51degrees.com/documentation/_device_detection__quickstart.html?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=find-out-more", example);
	fprintf(output, "\nContact us: ");
	writeLink(output, "https://51degrees.com/contact-us?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-c-hash-examplebase.c&utm_term=find-out-more", example);
	fprintf(output, "\nSource code: https://github.com/51Degrees/51degrees-cxx\n");
}

// Memory used to read one record of an evidence file.
#define EXAMPLE_EVIDENCE_PAIRS 20
#define EXAMPLE_EVIDENCE_KEY_SIZE 200
#define EXAMPLE_EVIDENCE_VALUE_SIZE 1000

// The key of the User-Agent in a record of an evidence file.
#define EXAMPLE_EVIDENCE_USER_AGENT_KEY "header.user-agent"

/**
 * The buffer, state and callback given to
 * fiftyoneDegreesExampleUserAgentsIterateWithLimit, which the callback for
 * each record needs.
 */
typedef struct example_user_agents_state_t {
	char *buffer;
	size_t length;
	void *state;
	void(*callback)(const char*, void*);
} exampleUserAgentsState;

/**
 * Called with each record of the evidence file. Copies the record's
 * User-Agent, when it has one, to the buffer and calls the callback with it.
 * @param pairs keys and values of the record
 * @param size number of pairs in the record
 * @param state instance of exampleUserAgentsState
 */
static void exampleUserAgentsRecord(
	KeyValuePair *pairs,
	uint16_t size,
	void *state) {
	exampleUserAgentsState *userAgents = (exampleUserAgentsState*)state;
	for (uint16_t i = 0; i < size; i++) {
		if (StringCompare(
			pairs[i].key,
			EXAMPLE_EVIDENCE_USER_AGENT_KEY) == 0) {
			size_t length = strlen(pairs[i].value);
			if (length > userAgents->length - 1) {
				length = userAgents->length - 1;
			}
			memcpy(userAgents->buffer, pairs[i].value, length);
			userAgents->buffer[length] = '\0';
			userAgents->callback(userAgents->buffer, userAgents->state);
			break;
		}
	}
}

void fiftyoneDegreesExampleUserAgentsIterateWithLimit(
	const char *fileName,
	char *buffer,
	int length,
	int limit,
	void *state,
	void(*callback)(const char*, void*)) {
	char fileBuffer[1000];
	KeyValuePair pairs[EXAMPLE_EVIDENCE_PAIRS];
	char keys[EXAMPLE_EVIDENCE_PAIRS][EXAMPLE_EVIDENCE_KEY_SIZE];
	char values[EXAMPLE_EVIDENCE_PAIRS][EXAMPLE_EVIDENCE_VALUE_SIZE];
	exampleUserAgentsState userAgents;
	if (length <= 0) {
		return;
	}
	for (int i = 0; i < EXAMPLE_EVIDENCE_PAIRS; i++) {
		pairs[i].key = keys[i];
		pairs[i].keyLength = EXAMPLE_EVIDENCE_KEY_SIZE;
		pairs[i].value = values[i];
		pairs[i].valueLength = EXAMPLE_EVIDENCE_VALUE_SIZE;
	}
	userAgents.buffer = buffer;
	userAgents.length = (size_t)length;
	userAgents.state = state;
	userAgents.callback = callback;
	YamlFileIterateWithLimit(
		fileName,
		fileBuffer,
		sizeof(fileBuffer),
		pairs,
		EXAMPLE_EVIDENCE_PAIRS,
		limit,
		&userAgents,
		exampleUserAgentsRecord);
}

void fiftyoneDegreesExampleUserAgentsIterate(
	const char *fileName,
	char *buffer,
	int length,
	void *state,
	void(*callback)(const char*, void*)) {
	fiftyoneDegreesExampleUserAgentsIterateWithLimit(
		fileName,
		buffer,
		length,
		-1,
		state,
		callback);
}
