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
#include "../../src/ipi/EngineIpi.hpp"
#include "both.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

BothResult bothIpiEngineLookup(void) {
	char path[FIFTYONE_DEGREES_FILE_MAX_PATH];
	if (fiftyoneDegreesFileGetPath(
		"ip-intelligence-data",
		"51Degrees-LiteV41.ipi",
		path,
		sizeof(path)) != FIFTYONE_DEGREES_STATUS_SUCCESS) {
		return BOTH_NO_DATA;
	}
	try {
		ConfigIpi config;
		config.setMaxPerformance();
		RequiredPropertiesConfig properties("RegisteredCountry");
		EngineIpi engine(path, &config, &properties);
		std::unique_ptr<ResultsIpi> results(engine.process("185.28.167.77"));
		auto value = results->getValuesAsWeightedStringList(
			"RegisteredCountry");
		return value.hasValue() && !value.getValue().empty() ?
			BOTH_PASSED : BOTH_FAILED;
	}
	catch (const std::exception &e) {
		std::cout << e.what() << "\n";
		return BOTH_FAILED;
	}
}
