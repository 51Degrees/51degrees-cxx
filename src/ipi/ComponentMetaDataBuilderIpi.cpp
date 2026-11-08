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

#include "ComponentMetaDataBuilderIpi.hpp"

#include "fiftyone.h"
#include "../common/Exceptions.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

ComponentMetaData* ComponentMetaDataBuilderIpi::build(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t componentIndex) {
	uint8_t componentId = 0;
	uint32_t defaultProfileOffset = 0;
	if (IpiGetComponent(
		dataSet,
		componentIndex,
		&componentId,
		&defaultProfileOffset) == false) {
		return nullptr;
	}
	EXCEPTION_CREATE;
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	char * const name = IpiGetComponentName(
		dataSet,
		componentIndex,
		buffer,
		sizeof(buffer),
		exception);
	// The key used for profiles is the offset, as there is no profile id.
	return new ComponentMetaData(
		componentId,
		takeText(name, buffer, exception),
		defaultProfileOffset);
}