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

#include <utility>
#include "ValueMetaDataBuilderIpi.hpp"
#include "../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace std;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

ValueMetaData* ValueMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex) {
	return buildValue(dataSet, valueIndex, nullptr);
}

ValueMetaData* ValueMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	const string &propertyName) {
	return buildValue(dataSet, valueIndex, &propertyName);
}

ValueMetaData* ValueMetaDataBuilderIpi::buildValue(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	const string *propertyName) {
	EXCEPTION_CREATE;
	uint32_t propertyIndex = 0;
	ValueTexts texts;
	if (IpiGetValue(
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
	string name;
	if (propertyName != nullptr) {
		name = *propertyName;
	}
	else {
		char buffer[REASONABLE_WKT_STRING_LENGTH];
		char * const text = IpiGetPropertyText(
			dataSet,
			propertyIndex,
			FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
			buffer,
			sizeof(buffer),
			exception);
		if (text == nullptr) {
			EXCEPTION_THROW;
			return nullptr;
		}
		name = takeText(text, buffer, exception);
	}
	// Moved rather than copied, as a text can be long.
	return new ValueMetaData(
		ValueMetaDataKey(std::move(name), std::move(texts.name)),
		std::move(texts.description),
		std::move(texts.url));
}
