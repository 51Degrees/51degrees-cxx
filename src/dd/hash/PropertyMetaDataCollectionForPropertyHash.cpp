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

#include "PropertyMetaDataCollectionForPropertyHash.hpp"
#include "../../common/Exceptions.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

PropertyMetaDataCollectionForPropertyHash::PropertyMetaDataCollectionForPropertyHash(
	fiftyoneDegreesResourceManager* manager,
	PropertyMetaData *property) : Collection<string, PropertyMetaData>() {
	DataSetHash* dataSet = DataSetHashGet(manager);
	if (dataSet != nullptr) {
		vector<uint32_t> indexes = property->getEvidenceProperties();
		for (uint32_t i = 0; i < indexes.size(); i++) {
			PropertyMetaData * const evidence =
				PropertyMetaDataBuilderHash::build(dataSet, indexes[i]);
			if (evidence != nullptr) {
				properties.push_back(shared_ptr<PropertyMetaData>(evidence));
			}
		}

		DataSetHashRelease(dataSet);
	}
}

PropertyMetaDataCollectionForPropertyHash::
~PropertyMetaDataCollectionForPropertyHash() {
	properties.clear();
}

PropertyMetaData* PropertyMetaDataCollectionForPropertyHash::getByIndex(
	uint32_t index) const {
	return new PropertyMetaData(*properties.at(index));
}

PropertyMetaData* PropertyMetaDataCollectionForPropertyHash::getByKey(
	string name) const {
	PropertyMetaData* result = nullptr;
	for (vector<shared_ptr<PropertyMetaData>>::const_iterator i = properties.begin();
		i != properties.end();
		i++) {
		if (name == (*i)->getName()) {
			result = new PropertyMetaData(**i);
			break;
		}
	}
	return result;
}

uint32_t PropertyMetaDataCollectionForPropertyHash::getSize() const {
	return (uint32_t)properties.size();
}