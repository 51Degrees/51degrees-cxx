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

#include "ValueMetaDataCollectionForPropertyHash.hpp"
#include "../../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

ValueMetaDataCollectionForPropertyHash::ValueMetaDataCollectionForPropertyHash(
	fiftyoneDegreesResourceManager *manager,
	PropertyMetaData *property)
	: ValueMetaDataCollectionBaseHash(manager),
	propertyName(property->getName()),
	propertyIndex(-1),
	firstValueIndex(0),
	valueCount(0) {
	EXCEPTION_CREATE;
	propertyIndex = HashGetPropertyIndex(
		dataSet,
		propertyName.c_str(),
		exception);
	EXCEPTION_THROW;
	if (propertyIndex >= 0) {
		HashGetPropertyValueIndexes(
			dataSet,
			(uint32_t)propertyIndex,
			&firstValueIndex,
			&valueCount,
			nullptr,
			exception);
		EXCEPTION_THROW;
	}
}

ValueMetaDataCollectionForPropertyHash::~ValueMetaDataCollectionForPropertyHash() {
}

ValueMetaData* ValueMetaDataCollectionForPropertyHash::getByIndex(
	uint32_t index) const {
	return propertyIndex < 0
		? nullptr
		: ValueMetaDataBuilderHash::build(dataSet, firstValueIndex + index);
}

ValueMetaData* ValueMetaDataCollectionForPropertyHash::getByKey(
	ValueMetaDataKey key) const {
	// Only a value of this collection's property is in it.
	if (propertyIndex < 0 || key.getPropertyName() != propertyName) {
		return nullptr;
	}
	EXCEPTION_CREATE;
	const long valueIndex = HashGetValueIndex(
		dataSet,
		(uint32_t)propertyIndex,
		key.getValueName().c_str(),
		exception);
	EXCEPTION_THROW;
	return valueIndex < 0
		? nullptr
		: ValueMetaDataBuilderHash::build(dataSet, (uint32_t)valueIndex);
}

uint32_t ValueMetaDataCollectionForPropertyHash::getSize() const {
	return valueCount;
}