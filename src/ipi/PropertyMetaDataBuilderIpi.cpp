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

#include "PropertyMetaDataBuilderIpi.hpp"
#include "../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

PropertyMetaData* PropertyMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex) {
	EXCEPTION_CREATE;
	uint8_t valueType = 0, componentId = 0, displayOrder = 0;
	bool isMandatory = false, isList = false, isObsolete = false;
	bool show = false, showValues = false, isAvailable = false;
	uint32_t mapCount = 0;
	if (IpiGetProperty(
		dataSet,
		propertyIndex,
		&valueType,
		&componentId,
		&displayOrder,
		&isMandatory,
		&isList,
		&isObsolete,
		&show,
		&showValues,
		&mapCount,
		&isAvailable,
		nullptr,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	string name = getText(
		dataSet,
		propertyIndex,
		FIFTYONE_DEGREES_PROPERTY_TEXT_NAME);
	return new PropertyMetaData(
		name,
		getPropertyMap(dataSet, propertyIndex, mapCount),
		PropertyValueTypeGetName(
			(PropertyValueType)valueType,
			isList),
		getText(
			dataSet,
			propertyIndex,
			FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY),
		getText(dataSet, propertyIndex, FIFTYONE_DEGREES_PROPERTY_TEXT_URL),
		isAvailable,
		displayOrder,
		isMandatory,
		isList,
		isObsolete,
		show,
		showValues,
		getText(
			dataSet,
			propertyIndex,
			FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION),
		getText(
			dataSet,
			propertyIndex,
			FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE),
		componentId,
		getEvidenceProperties(dataSet, name));
}

string PropertyMetaDataBuilderIpi::getText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	fiftyoneDegreesPropertyText text) {
	EXCEPTION_CREATE;
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	char * const result = IpiGetPropertyText(
		dataSet,
		propertyIndex,
		text,
		buffer,
		sizeof(buffer),
		exception);
	return takeText(result, buffer, exception);
}

vector<uint32_t> PropertyMetaDataBuilderIpi::getEvidenceProperties(
	fiftyoneDegreesDataSetIpi * const dataSet,
	const string &name) {
	vector<uint32_t> indexes;
	const int index = PropertiesGetAvailableIndexFromName(
		dataSet->b.b.available,
		name.c_str());
	if (index >= 0) {
		fiftyoneDegreesEvidenceProperties* evidenceProperties =
			dataSet->b.b.available->items[index].evidenceProperties;
		indexes.reserve(evidenceProperties->count);
		for (uint32_t i = 0; i < evidenceProperties->count; i++) {
			indexes.push_back(evidenceProperties->items[i]);
		}
	}
	return indexes;
}

vector<string> PropertyMetaDataBuilderIpi::getPropertyMap(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint32_t mapCount) {
	vector<string> map;
	map.reserve(mapCount);
	for (uint32_t i = 0; i < mapCount; i++) {
		EXCEPTION_CREATE;
		char buffer[REASONABLE_WKT_STRING_LENGTH];
		char * const name = IpiGetPropertyMapText(
			dataSet,
			propertyIndex,
			i,
			buffer,
			sizeof(buffer),
			exception);
		if (name != nullptr) {
			map.push_back(takeText(name, buffer, exception));
		}
		else {
			EXCEPTION_THROW;
		}
	}
	return map;
}
