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

#include "ValueMetaDataCollectionBaseIpi.hpp"
#include "../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace std;
using namespace FiftyoneDegrees::IpIntelligence;

ValueMetaDataCollectionBaseIpi::ValueMetaDataCollectionBaseIpi(
	fiftyoneDegreesResourceManager *manager)
	: Collection<ValueMetaDataKey, ValueMetaData>() {
	dataSet = DataSetIpiGet(manager);
	if (dataSet == nullptr) {
		throw runtime_error("Data set pointer can not be null");
	}
}

ValueMetaDataCollectionBaseIpi::~ValueMetaDataCollectionBaseIpi() {
	DataSetIpiRelease(dataSet);
}

ValueMetaData* ValueMetaDataCollectionBaseIpi::getByKey(
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
	const long valueIndex = IpiGetValueIndex(
		dataSet,
		(uint32_t)propertyIndex,
		key.getValueName().c_str(),
		exception);
	EXCEPTION_THROW;
	return valueIndex < 0
		? nullptr
		: ValueMetaDataBuilderIpi::build(dataSet, (uint32_t)valueIndex);
}

ValueMetaData* ValueMetaDataCollectionBaseIpi::getDefaultForProperty(
	const std::string &propertyName) const {
	EXCEPTION_CREATE;
	const int propertyIndex = IpiGetPropertyIndex(
		dataSet,
		propertyName.c_str(),
		exception);
	EXCEPTION_THROW;
	if (propertyIndex < 0) {
		return nullptr;
	}
	long defaultValueIndex = -1;
	IpiGetPropertyValueIndexes(
		dataSet,
		(uint32_t)propertyIndex,
		nullptr,
		nullptr,
		&defaultValueIndex,
		exception);
	EXCEPTION_THROW;
	return defaultValueIndex < 0
		? nullptr
		: ValueMetaDataBuilderIpi::build(
			dataSet,
			(uint32_t)defaultValueIndex);
}