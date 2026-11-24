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

#include "ValueMetaDataCollectionForProfileHash.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

/**
 * Gets the value index at the position given in the list that follows the
 * profile. The list follows the packed profile, so it can start at any byte,
 * and each index is copied out by its position.
 */
static uint32_t getValueIndex(const Profile *profile, uint32_t position) {
	return UnalignedReadUint32(
		(const byte*)(profile + 1) + (size_t)position * sizeof(uint32_t));
}

/**
 * Returns the first position from first up to last whose value index the
 * predicate is false for, where the predicate partitions the value indexes
 * from first to last, as std::partition_point does.
 */
template <typename Predicate>
static uint32_t partitionPoint(
	const Profile *profile,
	uint32_t first,
	uint32_t last,
	Predicate predicate) {
	while (first < last) {
		const uint32_t middle = first + (last - first) / 2;
		if (predicate(getValueIndex(profile, middle))) {
			first = middle + 1;
		}
		else {
			last = middle;
		}
	}
	return first;
}

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
	const uint32_t valueIndex = getValueIndex(getProfile(), index);
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
	uint32_t firstValueIndex = 0, valueCount = 0;
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
	// A profile holds each property's values together, with the properties
	// in the order of their value indexes, but a data file can hold one
	// property's values within a profile out of that order.
	const Profile * const profile = getProfile();
	const uint32_t last = profile->valueCount;
	const uint32_t begin = partitionPoint(
		profile,
		0,
		last,
		[firstValueIndex](uint32_t index) {
			return index < firstValueIndex;
		});
	const uint32_t end = partitionPoint(
		profile,
		begin,
		last,
		[firstValueIndex, valueCount](uint32_t index) {
			return index - firstValueIndex < valueCount;
		});
	if (begin == end) {
		return nullptr;
	}
	const long valueIndex = HashGetValueIndex(
		dataSet,
		(uint32_t)propertyIndex,
		key.getValueName().c_str(),
		exception);
	EXCEPTION_THROW;
	if (valueIndex >= 0) {
		for (uint32_t position = begin; position < end; position++) {
			if (getValueIndex(profile, position) == (uint32_t)valueIndex) {
				return ValueMetaDataBuilderHash::build(
					dataSet,
					(uint32_t)valueIndex);
			}
		}
	}
	// When the profile does not hold the index found, the names in its block
	// are compared, because the search by name can miss a value that the
	// data file holds out of the byte order of its property's value names,
	// or find another value of the same name.
	for (uint32_t position = begin; position < end; position++) {
		ValueMetaData * const value = ValueMetaDataBuilderHash::build(
			dataSet,
			getValueIndex(profile, position),
			key.getPropertyName());
		if (value != nullptr && value->getName() == key.getValueName()) {
			return value;
		}
		delete value;
	}
	return nullptr;
}

uint32_t ValueMetaDataCollectionForProfileHash::getSize() const {
	return getProfile()->valueCount;
}

fiftyoneDegreesProfile* ValueMetaDataCollectionForProfileHash::getProfile() const {
	return (Profile*)profileItem.data.ptr;
}