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

#include "PropertyMetaDataCollectionHash.hpp"
#include "../../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

PropertyMetaDataCollectionHash::PropertyMetaDataCollectionHash(
	fiftyoneDegreesResourceManager *manager)
	: Collection<string, PropertyMetaData>() {
	dataSet = DataSetHashGet(manager);
	if (dataSet == nullptr) {
		throw runtime_error("Data set pointer can not be null");
	}
}

PropertyMetaDataCollectionHash::~PropertyMetaDataCollectionHash() {
	DataSetHashRelease(dataSet);
}

PropertyMetaData* PropertyMetaDataCollectionHash::getByIndex(
	uint32_t index) const {
	return PropertyMetaDataBuilderHash::build(dataSet, index);
}

PropertyMetaData* PropertyMetaDataCollectionHash::getByKey(string name) const {
	EXCEPTION_CREATE;
	const int index = HashGetPropertyIndex(dataSet, name.c_str(), exception);
	EXCEPTION_THROW;
	return index < 0
		? nullptr
		: PropertyMetaDataBuilderHash::build(dataSet, (uint32_t)index);
}

uint32_t PropertyMetaDataCollectionHash::getSize() const {
	return HashGetPropertyCount(dataSet);
}