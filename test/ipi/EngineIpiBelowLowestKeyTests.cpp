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

#include <initializer_list>
#include <memory>
#include <string>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"

using namespace std;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

// Committed to the data submodule, so the tests need no download.
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

// The default value of the Asn property, read from a profile with no values.
static const char noAsn[] = "Unknown";

static const char wrongAsn[] =
	" gave the wrong Asn, or the data file has a new lowest key.";

/**
 * An address below the lowest key in a graph takes the graph's lowest leaf,
 * which is the leaf of that key.
 */
class EngineIpiBelowLowestKeyTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		string dataFilePath = GetFilePath(_dataFolderName, asnFileName);
		ASSERT_FALSE(dataFilePath.empty()) << asnFileName << " not found.";
		engine = make_unique<Ipi::EngineIpi>(
			dataFilePath,
			&config,
			&properties);
	}
	void TearDown() override {
		engine.reset();
		Base::TearDown();
	}
protected:
	Ipi::ConfigIpi config;
	Common::RequiredPropertiesConfig properties;
	unique_ptr<Ipi::EngineIpi> engine;

	string asnOf(const char* ipAddress) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(ipAddress));
		return results->getValueAsString("Asn").getValue();
	}

	void expectAsnOfLowestKey(
		const char* lowestKey,
		initializer_list<const char*> addressesBelow) {
		const string expected = asnOf(lowestKey);
		// A lowest key with no values would match a wrong leaf with none.
		ASSERT_NE(noAsn, expected) << lowestKey << wrongAsn;
		for (const char* ipAddress : addressesBelow) {
			EXPECT_EQ(expected, asnOf(ipAddress)) << ipAddress << wrongAsn;
		}
	}
};

// These follow low entries before falling below a low limit.
TEST_F(EngineIpiBelowLowestKeyTests, Ipv4) {
	expectAsnOfLowestKey("1.0.0.0", { "0.0.0.0", "0.255.255.255" });
}

// The first falls below the low limit at the root, and the second after low
// entries.
TEST_F(EngineIpiBelowLowestKeyTests, Ipv6) {
	expectAsnOfLowestKey(
		"2001:4:112::",
		{ "::", "2001:4:111:ffff:ffff:ffff:ffff:ffff" });
}
