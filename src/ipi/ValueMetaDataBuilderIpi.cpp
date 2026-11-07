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
 * Get the string representation of the data stored in
 * the strings collection, with numbers written to DefaultWktDecimalPlaces.
 * A value that is read but cannot be written as text gives an empty string,
 * so the text is written here rather than taken from
 * fiftyoneDegreesValueGetText, which fails for such a value.
 * @param stringsCollection the string collection
 * @param offset the offset in the string collection
 * @param storedValueType format of byte array representation.
 */
static string getDynamicString(
	fiftyoneDegreesCollection *stringsCollection,
	uint32_t offset,
	fiftyoneDegreesPropertyValueType storedValueType) {
	EXCEPTION_CREATE;
	string result;
	Item item;
	const StoredBinaryValue *binaryValue;
	DataReset(&item.data);
	binaryValue = StoredBinaryValueGet(
		stringsCollection,
		offset,
		storedValueType,
		&item,
		exception);
	EXCEPTION_THROW;
	if (binaryValue != nullptr) {
		char buffer[REASONABLE_WKT_STRING_LENGTH];
		char * const text = StoredBinaryValueToText(
			binaryValue,
			storedValueType,
			DefaultWktDecimalPlaces,
			buffer,
			sizeof(buffer),
			exception);
		if (text != nullptr) {
			result = text;
			if (text != buffer) {
				Free(text);
			}
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(
			stringsCollection,
			&item);
	}
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
	PropertyValueType const storedValueType = PropertyGetStoredTypeByIndex(
		dataSet->propertyTypes,
		value->propertyIndex,
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
				getDynamicString(
					dataSet->strings,
					value->nameOffset,
					storedValueType)),
			"",
			url);
		COLLECTION_RELEASE(dataSet->properties, &item);
	}
	return result;
}
