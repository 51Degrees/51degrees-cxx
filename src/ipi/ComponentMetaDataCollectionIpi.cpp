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

#include "ComponentMetaDataCollectionIpi.hpp"
#include "fiftyone.h"

using namespace std;
using namespace FiftyoneDegrees::IpIntelligence;

ComponentMetaDataCollectionIpi::ComponentMetaDataCollectionIpi(
	fiftyoneDegreesResourceManager *manager)
	: Collection<::byte, ComponentMetaData>() {
	dataSet = DataSetIpiGet(manager);
	if (dataSet == nullptr) {
		throw runtime_error("Data set pointer can not be null");
	}
}

ComponentMetaDataCollectionIpi::~ComponentMetaDataCollectionIpi() {
	DataSetIpiRelease(dataSet);
}

ComponentMetaData* ComponentMetaDataCollectionIpi::getByIndex(
	uint32_t index) const {
	return ComponentMetaDataBuilderIpi::build(dataSet, index);
}

ComponentMetaData* ComponentMetaDataCollectionIpi::getByKey(
	::byte componentId) const {
	ComponentMetaData *result = nullptr;
	const uint32_t count = IpiGetComponentCount(dataSet);
	for (uint32_t i = 0; i < count; i++) {
		uint8_t id = 0;
		IpiGetComponent(dataSet, i, &id, nullptr);
		// The last component with the id is the one given.
		if (id == componentId) {
			delete result;
			result = ComponentMetaDataBuilderIpi::build(dataSet, i);
		}
	}
	return result;
}

uint32_t ComponentMetaDataCollectionIpi::getSize() const {
	return IpiGetComponentCount(dataSet);
}