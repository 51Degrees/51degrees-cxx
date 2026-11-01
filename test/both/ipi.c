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

#include <stdio.h>
#include <string.h>
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/fiftyone.h"
#include "both.h"

const void* bothIpiCommonGlobal(void) {
	return (const void*)&Malloc;
}

BothResult bothIpiCommonCall(void) {
	uint32_t source[2] = { 12, 0 };
	MemoryReader reader;
	reader.startByte = (byte*)source;
	reader.current = (byte*)source;
	reader.lastByte = reader.startByte + sizeof(source);
	reader.length = (FileOffset)sizeof(source);
	CollectionHeader header = CollectionHeaderFromMemory(&reader, 4, false);
	return header.length == 12u &&
		header.count == 3u &&
		header.startPosition == 4u ? BOTH_PASSED : BOTH_FAILED;
}

BothResult bothIpiLookup(void) {
	const char *address = "185.28.167.77";
	char path[FILE_MAX_PATH];
	char value[256] = "";
	ResourceManager manager;
	ConfigIpi config = IpiInMemoryConfig;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = "RegisteredCountry";
	EXCEPTION_CREATE;

	if (FileGetPath(
		"ip-intelligence-data",
		"51Degrees-LiteV41.ipi",
		path,
		sizeof(path)) != SUCCESS) {
		return BOTH_NO_DATA;
	}
	StatusCode status = IpiInitManagerFromFile(
		&manager,
		&config,
		&properties,
		path,
		exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return BOTH_FAILED;
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
	BothResult result = EXCEPTION_OKAY && length > 0 ?
		BOTH_PASSED : BOTH_FAILED;
	printf("IP Intelligence RegisteredCountry: %s\n", value);

	ResultsIpiFree(results);
	ResourceManagerFree(&manager);
	return result;
}
