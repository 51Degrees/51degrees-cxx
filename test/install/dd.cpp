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

/*
 * Uses the installed Device Detection C++ library. With a data file as its
 * argument it looks up a User-Agent and prints IsMobile. Without one it checks
 * that a file that does not exist is reported as not found.
 */

#include <exception>
#include <iostream>
#include <memory>
#include <51degrees/dd/hash/EngineHash.hpp>

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

int main(int argc, char *argv[]) {
	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig properties("IsMobile");
	try {
		EngineHash engine(
			argc > 1 ? argv[1] : "missing.hash",
			&config,
			&properties);
		if (argc < 2) {
			std::cout << "A data file that does not exist was loaded\n";
			return 1;
		}
		EvidenceDeviceDetection evidence;
		evidence["header.user-agent"] =
			"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
			"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 "
			"Mobile/11D167 Safari/9537.53";
		std::unique_ptr<ResultsHash> results(engine.process(&evidence));
		Value<bool> value = results->getValueAsBool("IsMobile");
		bool mobile = value.hasValue() && value.getValue();
		std::cout << "IsMobile: " << (mobile ? "True" : "False") << "\n";
		return mobile ? 0 : 1;
	}
	catch (const StatusCodeException &e) {
		std::cout << e.what() << "\n";
		return argc < 2 &&
			e.getCode() == FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND ? 0 : 1;
	}
	catch (const std::exception &e) {
		std::cout << e.what() << "\n";
		return 1;
	}
}
