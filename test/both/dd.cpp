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

#include <exception>
#include <iostream>
#include <memory>
#include "../../src/dd/hash/EngineHash.hpp"
#include "both.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

BothResult bothDdEngineLookup(void) {
	char path[FIFTYONE_DEGREES_FILE_MAX_PATH];
	if (fiftyoneDegreesFileGetPath(
		"device-detection-data",
		"51Degrees-LiteV4.1.hash",
		path,
		sizeof(path)) != FIFTYONE_DEGREES_STATUS_SUCCESS) {
		return BOTH_NO_DATA;
	}
	try {
		ConfigHash config;
		config.setMaxPerformance();
		RequiredPropertiesConfig properties("IsMobile");
		EngineHash engine(path, &config, &properties);
		EvidenceDeviceDetection evidence;
		evidence["header.user-agent"] =
			"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
			"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 "
			"Mobile/11D167 Safari/9537.53";
		std::unique_ptr<ResultsHash> results(engine.process(&evidence));
		Value<bool> value = results->getValueAsBool("IsMobile");
		return value.hasValue() && value.getValue() ?
			BOTH_PASSED : BOTH_FAILED;
	}
	catch (const std::exception &e) {
		std::cout << e.what() << "\n";
		return BOTH_FAILED;
	}
}
