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

#include "ValueMetaDataCollectionIpi.hpp"
#include "../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::IpIntelligence;

ValueMetaDataCollectionIpi::ValueMetaDataCollectionIpi(
	fiftyoneDegreesResourceManager *manager)
	: ValueMetaDataCollectionBaseIpi(manager) {
}

ValueMetaDataCollectionIpi::~ValueMetaDataCollectionIpi() {}

ValueMetaData* ValueMetaDataCollectionIpi::getByIndex(uint32_t index) const {
	std::call_once(propertyNamesRead, [this]() { readPropertyNames(); });
	return ValueMetaDataBuilderIpi::build(dataSet, index, propertyNames);
}

void ValueMetaDataCollectionIpi::readPropertyNames() const {
	const uint32_t count = IpiGetPropertyCount(dataSet);
	std::vector<std::string> names;
	names.reserve(count);
	for (uint32_t i = 0; i < count; i++) {
		EXCEPTION_CREATE;
		char buffer[REASONABLE_WKT_STRING_LENGTH];
		char * const text = IpiGetPropertyText(
			dataSet,
			i,
			FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
			buffer,
			sizeof(buffer),
			exception);
		EXCEPTION_THROW;
		names.push_back(text == nullptr ? std::string() : std::string(text));
		if (text != nullptr && text != buffer) {
			Free(text);
		}
	}
	propertyNames = std::move(names);
}

ValueMetaData* ValueMetaDataCollectionIpi::getByKey(ValueMetaDataKey key) const {
	return ValueMetaDataCollectionBaseIpi::getByKey(key);
}

uint32_t ValueMetaDataCollectionIpi::getSize() const {
	return CollectionGetCount(dataSet->values);
}