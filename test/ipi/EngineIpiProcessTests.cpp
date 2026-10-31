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

#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "Constants.hpp"
#include <string>

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

static const char processAsnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

static decltype(fiftyoneDegreesMalloc) processOriginalMalloc = nullptr;
static bool processRefuse = false;

// Refuses the first allocation after it is set.
static void* FIFTYONE_DEGREES_CALL_CONV processRefusingMalloc(size_t size) {
	if (processRefuse) {
		processRefuse = false;
		return nullptr;
	}
	return processOriginalMalloc(size);
}

class EngineIpiProcessTests : public Base {};

// With no memory for the results the caller gets an exception, not a crash.
TEST_F(EngineIpiProcessTests, noMemoryForResultsThrows) {
	const std::string path = GetFilePath(_dataFolderName, processAsnFileName);
	if (path.empty()) {
		GTEST_SKIP() << "The file '" << processAsnFileName << "' was not found.";
	}
	ConfigIpi config;
	RequiredPropertiesConfig properties("Asn");
	EngineIpi engine(path.c_str(), &config, &properties);

	processOriginalMalloc = fiftyoneDegreesMalloc;
	processRefuse = true;
	fiftyoneDegreesMalloc = processRefusingMalloc;
	EXPECT_THROW(engine.process("8.8.8.8"), FatalException);
	fiftyoneDegreesMalloc = processOriginalMalloc;
}
