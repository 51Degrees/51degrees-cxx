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

#include "ValueMetaDataBuilderIpi.hpp"
#include "../common/Exceptions.hpp"
#include "constantsIpi.h"
#include "fiftyone.h"

using namespace std;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

/* Maximum buffer length to hold an IP address string */
#define IP_ADDRESS_STRING_MAX_LENGTH 50
/* Coordinate floating point precision */
#define COORDINATE_PRECISION 7

/**
 * Get the text of the value's name, read as the type its property is stored
 * as and with numbers written to DefaultWktDecimalPlaces, as
 * fiftyoneDegreesValueGetText gives it. A name that is read but cannot be
 * written is an empty string.
 * @param dataSet the data set the value is from
 * @param value the value to get the name of
 */
static string getDynamicString(
	fiftyoneDegreesDataSetIpi *dataSet,
	const fiftyoneDegreesValue *value) {
	EXCEPTION_CREATE;
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	char * const text = ValueGetText(
		dataSet->strings,
		dataSet->propertyTypes,
		value,
		DefaultWktDecimalPlaces,
		buffer,
		sizeof(buffer),
		exception);
	string result;
	if (text != nullptr) {
		result = text;
		if (text != buffer) {
			Free(text);
		}
	}
	EXCEPTION_THROW;
	return result;
}



ValueMetaData* ValueMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	const fiftyoneDegreesValue *value) {
	EXCEPTION_CREATE;
	ValueMetaData *result = nullptr;
	Item item;
	Property *property;
	DataReset(&item.data);
	property = PropertyGet(
		dataSet->properties,
		value->propertyIndex, 
		&item,
		exception);
	EXCEPTION_THROW;
	if (property != nullptr) {
		// Get URL only if not weighted (weighted values use urlOffsetOrWeight for weight)
		// and if there is one, as offset 0 holds the first string in the file.
		string url;
		if (!ValueIsWeighted(value) && value->urlOffsetOrWeight > 0) {
			url = getString(dataSet->strings, value->urlOffsetOrWeight);
		}
		result = new ValueMetaData(
			ValueMetaDataKey(
				getString(dataSet->strings, property->nameOffset),
				getDynamicString(dataSet, value)),
			"",
			url);
		COLLECTION_RELEASE(dataSet->properties, &item);
	}
	return result;
}
