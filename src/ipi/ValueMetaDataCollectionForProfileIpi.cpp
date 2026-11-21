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
#include "ValueMetaDataCollectionForProfileIpi.hpp"
#include "fiftyone.h"
#include "../common/collectionKeyTypes.h"

using namespace FiftyoneDegrees::IpIntelligence;

ValueMetaDataCollectionForProfileIpi::ValueMetaDataCollectionForProfileIpi(
	fiftyoneDegreesResourceManager *manager,
	ProfileMetaData *profile) : ValueMetaDataCollectionBaseIpi(manager) {
	EXCEPTION_CREATE;
	DataReset(&profileItem.data);
	// The key of an IP Intelligence profile is its offset in the profiles
	// collection.
	const CollectionKey profileKey = {
		profile->getKey(),
		CollectionKeyType_Profile,
	};
	dataSet->profiles->get(
		dataSet->profiles,
		&profileKey,
		&profileItem,
		exception);
	EXCEPTION_THROW;
}

ValueMetaDataCollectionForProfileIpi::~ValueMetaDataCollectionForProfileIpi() {
	COLLECTION_RELEASE(dataSet->profiles, &profileItem);
}

ValueMetaData* ValueMetaDataCollectionForProfileIpi::getByIndex(
	uint32_t index) const {
	const uint32_t valueIndex = ((uint32_t*)(getProfile() + 1))[index];
	return ValueMetaDataBuilderIpi::build(dataSet, valueIndex);
}

ValueMetaData* ValueMetaDataCollectionForProfileIpi::getByKey(
	ValueMetaDataKey key) const {
	EXCEPTION_CREATE;
	const int propertyIndex = IpiGetPropertyIndex(
		dataSet,
		key.getPropertyName().c_str(),
		exception);
	EXCEPTION_THROW;
	if (propertyIndex < 0) {
		return nullptr;
	}
	uint32_t firstValueIndex = 0;
	uint32_t valueCount = 0;
	if (IpiGetPropertyValueIndexes(
		dataSet,
		(uint32_t)propertyIndex,
		&firstValueIndex,
		&valueCount,
		nullptr,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	// The profile's own values of the property are compared by name, as a
	// weighted property can hold several values with the same name, and a
	// search of all the property's values can find another of them. The
	// profile's value indexes are in ascending order, so its values of the
	// property are the run within the property's range.
	const Profile * const profile = getProfile();
	const uint32_t * const first = (const uint32_t*)(profile + 1);
	const uint32_t * const last = first + profile->valueCount;
	const uint32_t * const begin =
		std::lower_bound(first, last, firstValueIndex);
	const uint32_t * const end =
		std::lower_bound(begin, last, firstValueIndex + valueCount);
	const string &valueName = key.getValueName();
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	for (const uint32_t *valueIndex = begin; valueIndex < end; valueIndex++) {
		char * const text = IpiGetValueText(
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
			return ValueMetaDataBuilderIpi::build(
				dataSet,
				*valueIndex,
				key.getPropertyName());
		}
	}
	return nullptr;
}

uint32_t ValueMetaDataCollectionForProfileIpi::getSize() const {
	return getProfile()->valueCount;
}

fiftyoneDegreesProfile* 
ValueMetaDataCollectionForProfileIpi::getProfile() const {
	return (Profile*)profileItem.data.ptr;
}