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
 * Uses the installed IP Intelligence C++ library. With a data file as its
 * argument it looks up an IP address and prints how many RegisteredCountry
 * values it has. Without one it checks that a file that does not exist is
 * reported as not found.
 */

/* The installed targets must bring the definitions IP Intelligence's copy of
 * common is compiled with, as they change the size of its structures. */
#if !defined(FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT) || \
	!defined(FIFTYONE_DEGREES_REDUCED_FILE)
#error "The IP Intelligence targets do not carry their compile definitions"
#endif

#include <exception>
#include <iostream>
#include <memory>
#include <51degrees/ipi/EngineIpi.hpp>

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

int main(int argc, char *argv[]) {
	ConfigIpi config;
	config.setLowMemory();
	RequiredPropertiesConfig properties("RegisteredCountry");
	try {
		EngineIpi engine(
			argc > 1 ? argv[1] : "missing.ipi",
			&config,
			&properties);
		if (argc < 2) {
			std::cout << "A data file that does not exist was loaded\n";
			return 1;
		}
		std::unique_ptr<ResultsIpi> results(engine.process("185.28.167.77"));
		auto value = results->getValuesAsWeightedStringList(
			"RegisteredCountry");
		size_t count = value.hasValue() ? value.getValue().size() : 0;
		std::cout << "RegisteredCountry values: " << count << "\n";
		return count > 0 ? 0 : 1;
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
