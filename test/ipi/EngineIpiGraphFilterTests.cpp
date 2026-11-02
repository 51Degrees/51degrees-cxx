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

#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using namespace std;
// Aliases rather than open namespaces, because the C synonym typedefs from
// fiftyone.h share their short names with the C++ classes.
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

// A public address from the evidence file that ships with the test data.
static const char* graphFilterIpAddress = "50.154.29.201";

/**
 * C++ tests for getRequiredProperties.
 */
class EngineIpiGraphFilterTests : public Base {
public:
	void SetUp() {
		Base::SetUp();
		string dataFilePath = "";
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath == "";
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		config = new Ipi::ConfigIpi();
		// Balanced rather than the in memory default, so a test does not
		// hold the whole data file, which is several gigabytes for the
		// enterprise file.
		config->setBalanced();
		properties = new Common::RequiredPropertiesConfig();
		engine = new Ipi::EngineIpi(dataFilePath, config, properties);
	}
	void TearDown() {
		delete engine;
		delete properties;
		delete config;
		Base::TearDown();
	}
protected:
	Ipi::ConfigIpi* config = nullptr;
	Common::RequiredPropertiesConfig* properties = nullptr;
	Ipi::EngineIpi* engine = nullptr;
};

TEST_F(EngineIpiGraphFilterTests, RequiredPropertiesAreInIndexOrder) {
	vector<string> names = engine->getRequiredProperties();
	unique_ptr<Ipi::ResultsIpi> results(engine->process(graphFilterIpAddress));
	vector<string> fromResults = results->getProperties();
	ASSERT_EQ(fromResults.size(), names.size());
	for (size_t i = 0; i < names.size(); i++) {
		EXPECT_EQ(fromResults[i], names[i]) <<
			"Name at required property index " << i << " differs.";
	}
	EXPECT_GT(names.size(), 0u);
}
