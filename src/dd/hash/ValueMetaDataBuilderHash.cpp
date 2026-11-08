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

#include "ValueMetaDataBuilderHash.hpp"
#include "../../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

ValueMetaData* ValueMetaDataBuilderHash::build(
	fiftyoneDegreesDataSetHash *dataSet,
	uint32_t valueIndex) {
	EXCEPTION_CREATE;
	uint32_t propertyIndex = 0;
	ValueTexts texts;
	if (HashGetValue(
		dataSet,
		valueIndex,
		&propertyIndex,
		&texts,
		addValueText,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	if (texts.failed) {
		throw StatusCodeException(INSUFFICIENT_MEMORY);
	}
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	char * const propertyName = HashGetPropertyText(
		dataSet,
		propertyIndex,
		FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
		buffer,
		sizeof(buffer),
		exception);
	if (propertyName == nullptr) {
		EXCEPTION_THROW;
		return nullptr;
	}
	const string name = takeText(propertyName, buffer, exception);
	return new ValueMetaData(
		ValueMetaDataKey(name, texts.name),
		texts.description,
		texts.url);
}
