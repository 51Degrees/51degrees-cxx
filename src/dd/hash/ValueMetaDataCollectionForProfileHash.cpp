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

#include <algorithm>
#include "ValueMetaDataCollectionForProfileHash.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

ValueMetaDataCollectionForProfileHash::ValueMetaDataCollectionForProfileHash(
	fiftyoneDegreesResourceManager *manager,
	ProfileMetaData *profile) : ValueMetaDataCollectionBaseHash(manager) {
	EXCEPTION_CREATE;
	DataReset(&profileItem.data);
	ProfileGetByProfileId(
		dataSet->profileOffsets,
		dataSet->profiles,
		profile->getProfileId(),
		&profileItem,
		exception);
	EXCEPTION_THROW;
}

ValueMetaDataCollectionForProfileHash::~ValueMetaDataCollectionForProfileHash() {
	COLLECTION_RELEASE(dataSet->profiles, &profileItem);
}

ValueMetaData* ValueMetaDataCollectionForProfileHash::getByIndex(
	uint32_t index) const {
	const uint32_t valueIndex = ((uint32_t*)(getProfile() + 1))[index];
	return ValueMetaDataBuilderHash::build(dataSet, valueIndex);
}

ValueMetaData* ValueMetaDataCollectionForProfileHash::getByKey(
	ValueMetaDataKey key) const {
	EXCEPTION_CREATE;
	const int propertyIndex = HashGetPropertyIndex(
		dataSet,
		key.getPropertyName().c_str(),
		exception);
	EXCEPTION_THROW;
	if (propertyIndex < 0) {
		return nullptr;
	}
	uint32_t firstValueIndex = 0;
	uint32_t valueCount = 0;
	if (HashGetPropertyValueIndexes(
		dataSet,
		(uint32_t)propertyIndex,
		&firstValueIndex,
		&valueCount,
		nullptr,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	if (valueCount == 0) {
		return nullptr;
	}
	// The profile's own values of the property are compared by name, rather
	// than the property's values searched by name, as that search relies on
	// the data file holding the names in byte order, which not every data
	// file does. The profile's value indexes are in ascending order, so its
	// values of the property are the run within the property's range.
	const Profile * const profile = getProfile();
	const uint32_t * const first = (const uint32_t*)(profile + 1);
	const uint32_t * const last = first + profile->valueCount;
	const uint32_t * const begin =
		std::lower_bound(first, last, firstValueIndex);
	const uint32_t * const end =
		std::upper_bound(begin, last, firstValueIndex + valueCount - 1);
	const string valueName = key.getValueName();
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	for (const uint32_t *valueIndex = begin; valueIndex < end; valueIndex++) {
		char * const text = HashGetValueText(
			dataSet,
			*valueIndex,
			FIFTYONE_DEGREES_VALUE_TEXT_NAME,
			buffer,
			sizeof(buffer),
			exception);
		if (text == nullptr) {
			EXCEPTION_THROW;
			continue;
		}
		const bool same = valueName == text;
		if (text != buffer) {
			Free(text);
		}
		if (same) {
			return ValueMetaDataBuilderHash::build(
				dataSet,
				*valueIndex,
				key.getPropertyName());
		}
	}
	return nullptr;
}

uint32_t ValueMetaDataCollectionForProfileHash::getSize() const {
	return getProfile()->valueCount;
}

fiftyoneDegreesProfile* ValueMetaDataCollectionForProfileHash::getProfile() const {
	return (Profile*)profileItem.data.ptr;
}