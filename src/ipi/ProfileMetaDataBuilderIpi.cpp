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

#include "ProfileMetaDataBuilderIpi.hpp"

#include "fiftyone.h"
#include "../common/Exceptions.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

ProfileMetaData* ProfileMetaDataBuilderIpi::buildByIndex(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t profileIndex) {
	EXCEPTION_CREATE;
	uint32_t profileOffset = 0;
	uint8_t componentId = 0;
	if (IpiGetProfile(
		dataSet,
		profileIndex,
		&profileOffset,
		&componentId,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	// The profile's offset is its key, as the id is not written to the data
	// file, and the id is not unique between data files either.
	return new ProfileMetaData(profileOffset, componentId);
}

ProfileMetaData* ProfileMetaDataBuilderIpi::buildByOffset(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t profileOffset) {
	EXCEPTION_CREATE;
	uint8_t componentId = 0;
	if (IpiGetProfileByOffset(
		dataSet,
		profileOffset,
		&componentId,
		exception) == false) {
		EXCEPTION_THROW;
		return nullptr;
	}
	return new ProfileMetaData(profileOffset, componentId);
}

ProfileMetaData* ProfileMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	const fiftyoneDegreesProfile *profile,
	const uint32_t profileOffset) {
	::byte componentId = ((Component*)dataSet->componentsList
		.items[profile->componentIndex].data.ptr)->componentId;
	return new ProfileMetaData(
		// Profile offset is used as the unique key, as the id is not written
		// to the data file, and the id is not unique between data files either.
		profileOffset,
		componentId);
}
