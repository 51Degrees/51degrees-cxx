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

/*
 * Uses the installed Device Detection C library. With a data file as its
 * argument it looks up a User-Agent and prints IsMobile. Without one it checks
 * that a file that does not exist is reported as not found.
 */

#include <stdio.h>
#include <string.h>
#include <51degrees/dd/hash/hash.h>
#include <51degrees/dd/hash/fiftyone.h>

int main(int argc, char *argv[]) {
	const char *userAgent =
		"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
		"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 "
		"Mobile/11D167 Safari/9537.53";
	char value[64] = "";
	ResourceManager manager;
	ConfigHash config = HashLowMemoryConfig;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = "IsMobile";
	EXCEPTION_CREATE;

	StatusCode status = HashInitManagerFromFile(
		&manager,
		&config,
		&properties,
		argc > 1 ? argv[1] : "missing.hash",
		exception);
	if (argc < 2) {
		printf("Status for a missing data file: %d\n", (int)status);
		return status == FILE_NOT_FOUND ? 0 : 1;
	}
	if (status != SUCCESS || EXCEPTION_FAILED) {
		printf("The data file was not loaded, status %d\n", (int)status);
		return 1;
	}

	ResultsHash *results = ResultsHashCreate(&manager, 1);
	ResultsHashFromUserAgent(results, userAgent, strlen(userAgent), exception);
	if (EXCEPTION_OKAY) {
		ResultsHashGetValuesString(
			results,
			"IsMobile",
			value,
			sizeof(value),
			",",
			exception);
	}
	int failed = EXCEPTION_FAILED || strcmp(value, "True") != 0;
	printf("IsMobile: %s\n", value);

	ResultsHashFree(results);
	ResourceManagerFree(&manager);
	return failed;
}
