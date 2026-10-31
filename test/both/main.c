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
#include "both.h"

/* CTest reports this return code as skipped. See CMakeLists.txt. */
#define SKIPPED_RETURN_CODE 77

static int failures = 0;
static int missingData = 0;

static void report(const char *check, BothResult result) {
	switch (result) {
	case BOTH_PASSED:
		printf("passed: %s\n", check);
		break;
	case BOTH_NO_DATA:
		printf("no data file: %s\n", check);
		missingData++;
		break;
	default:
		printf("FAILED: %s\n", check);
		failures++;
		break;
	}
}

int main(void) {
	report(
		"each product has its own common globals",
		bothDdCommonGlobal() != bothIpiCommonGlobal() ?
		BOTH_PASSED : BOTH_FAILED);
	report("Device Detection calls its own common", bothDdCommonCall());
	report("IP Intelligence calls its own common", bothIpiCommonCall());
	report("Device Detection lookup", bothDdLookup());
	report("IP Intelligence lookup", bothIpiLookup());

	if (failures > 0) {
		return 1;
	}
	return missingData > 0 ? SKIPPED_RETURN_CODE : 0;
}
