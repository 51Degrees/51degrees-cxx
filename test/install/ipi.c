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
 * Uses the installed IP Intelligence C library. With a data file as its
 * argument it looks up an IP address and prints RegisteredCountry. Without one
 * it checks that a file that does not exist is reported as not found.
 */

/* The installed targets must bring the definitions IP Intelligence's copy of
 * common is compiled with, as they change the size of its structures. */
#if !defined(FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT) || \
	!defined(FIFTYONE_DEGREES_REDUCED_FILE)
#error "The IP Intelligence targets do not carry their compile definitions"
#endif

#include <stdio.h>
#include <string.h>
#include <51degrees/ipi/ipi.h>
#include <51degrees/ipi/fiftyone.h>

int main(int argc, char *argv[]) {
	const char *address = "185.28.167.77";
	char value[256] = "";
	ResourceManager manager;
	ConfigIpi config = IpiLowMemoryConfig;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = "RegisteredCountry";
	EXCEPTION_CREATE;

	StatusCode status = IpiInitManagerFromFile(
		&manager,
		&config,
		&properties,
		argc > 1 ? argv[1] : "missing.ipi",
		exception);
	if (argc < 2) {
		printf("Status for a missing data file: %d\n", (int)status);
		return status == FILE_NOT_FOUND ? 0 : 1;
	}
	if (status != SUCCESS || EXCEPTION_FAILED) {
		printf("The data file was not loaded, status %d\n", (int)status);
		return 1;
	}

	ResultsIpi *results = ResultsIpiCreate(&manager);
	ResultsIpiFromIpAddressString(
		results,
		address,
		strlen(address),
		exception);
	size_t length = 0;
	if (EXCEPTION_OKAY) {
		length = ResultsIpiGetValuesString(
			results,
			"RegisteredCountry",
			value,
			sizeof(value),
			"|",
			exception);
	}
	int failed = EXCEPTION_FAILED || length == 0;
	printf("RegisteredCountry: %s\n", value);

	ResultsIpiFree(results);
	ResourceManagerFree(&manager);
	return failed;
}
