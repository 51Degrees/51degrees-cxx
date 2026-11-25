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
 

/**
 * @file EnumFromInteger.h
 * @brief Calls library functions that take an enum with any integer. C++
 * gives undefined behavior for a value outside an enum's range, where C
 * converts the integer to the enum's integer type, so tests of how the
 * library answers an unknown status or text make the call in C.
 */

#ifndef FIFTYONE_DEGREES_TEST_ENUM_FROM_INTEGER_H_INCLUDED
#define FIFTYONE_DEGREES_TEST_ENUM_FROM_INTEGER_H_INCLUDED

#include "../../src/common/status.h"
#include "../../src/common/property.h"
#include "../../src/common/value.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calls #fiftyoneDegreesStatusGetMessage with the status as an integer.
 */
const char* testStatusGetMessageFromInteger(int status, const char *fileName);

/**
 * Calls #fiftyoneDegreesPropertyGetText with the text as an integer.
 */
char* testPropertyGetTextFromInteger(
	const fiftyoneDegreesCollection *values,
	const fiftyoneDegreesCollection *strings,
	const fiftyoneDegreesCollection *propertyTypes,
	const fiftyoneDegreesProperty *property,
	int text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Calls #fiftyoneDegreesValueGetMetaDataText with the text as an integer.
 */
char* testValueGetMetaDataTextFromInteger(
	const fiftyoneDegreesCollection *strings,
	const fiftyoneDegreesCollection *propertyTypes,
	const fiftyoneDegreesValue *value,
	int text,
	uint8_t decimalPlaces,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

#ifdef __cplusplus
}
#endif

#endif
