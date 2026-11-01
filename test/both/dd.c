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
#include "../../src/dd/hash/hash.h"
#include "../../src/dd/hash/fiftyone.h"
#include "both.h"

const void* bothDdCommonGlobal(void) {
	return (const void*)&Malloc;
}

BothResult bothDdCommonCall(void) {
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

BothResult bothDdLookup(void) {
	const char *userAgent =
		"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
		"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 "
		"Mobile/11D167 Safari/9537.53";
	char path[FILE_MAX_PATH];
	char value[64] = "";
	ResourceManager manager;
	ConfigHash config = HashInMemoryConfig;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = "IsMobile";
	EXCEPTION_CREATE;

	if (FileGetPath(
		"device-detection-data",
		"51Degrees-LiteV4.1.hash",
		path,
		sizeof(path)) != SUCCESS) {
		return BOTH_NO_DATA;
	}
	StatusCode status = HashInitManagerFromFile(
		&manager,
		&config,
		&properties,
		path,
		exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return BOTH_FAILED;
	}

	ResultsHash *results = ResultsHashCreate(&manager, 1);
	ResultsHashFromUserAgent(
		results,
		userAgent,
		strlen(userAgent),
		exception);
	if (EXCEPTION_OKAY) {
		ResultsHashGetValuesString(
			results,
			"IsMobile",
			value,
			sizeof(value),
			(char* const)",",
			exception);
	}
	BothResult result = EXCEPTION_OKAY && strcmp(value, "True") == 0 ?
		BOTH_PASSED : BOTH_FAILED;
	printf("Device Detection IsMobile: %s\n", value);

	ResultsHashFree(results);
	ResourceManagerFree(&manager);
	return result;
}
