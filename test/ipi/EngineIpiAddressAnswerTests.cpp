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
#include <sstream>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using namespace std;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

// Committed to the data submodule, so these tests need no download. The lowest
// key in its IPv4 graph is 1.0.0.0 and in its IPv6 graph is 2001:4:112::.
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

// What a result holds when no profile relates to the address.
static const uint32_t noProfile = UINT32_MAX;

// Asn is mandatory with this default, so reads as it when there is no profile.
static const string defaultAsn = "Unknown";

static const char dataHint[] = " Has the data file gained a new lowest key?";

/**
 * What an address gets, read through the C API for the graph result and
 * through C++ for what a language sees.
 */
class IpiAddressAnswerTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		string dataFilePath = GetFilePath(_dataFolderName, asnFileName);
		ASSERT_FALSE(dataFilePath.empty()) << asnFileName << " not found.";
		EXCEPTION_CREATE;
		fiftyoneDegreesIpiInitManagerFromFile(
			&manager,
			&cConfig,
			&cProperties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
		engine = make_unique<Ipi::EngineIpi>(
			dataFilePath,
			&config,
			&properties);
	}
	void TearDown() override {
		engine.reset();
		if (manager.active != nullptr) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		Base::TearDown();
	}
protected:
	fiftyoneDegreesResourceManager manager = {};
	fiftyoneDegreesConfigIpi cConfig = fiftyoneDegreesIpiDefaultConfig;
	fiftyoneDegreesPropertiesRequired cProperties =
		fiftyoneDegreesPropertiesDefault;
	Ipi::ConfigIpi config;
	Common::RequiredPropertiesConfig properties;
	unique_ptr<Ipi::EngineIpi> engine;

	// The graph result for each component. An exception would fail the test.
	vector<uint32_t> rawOffsets(const char* ipAddress) {
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			ipAddress,
			strlen(ipAddress),
			exception);
		vector<uint32_t> offsets;
		for (uint32_t i = 0; i < results->count; i++) {
			offsets.push_back(results->items[i].graphResult.rawOffset);
		}
		fiftyoneDegreesResultsIpiFree(results);
		EXCEPTION_THROW;
		return offsets;
	}

	// Every property, as a language reads them.
	string propertiesOf(Ipi::ResultsIpi* results) {
		stringstream properties;
		for (const string& name : engine->getRequiredProperties()) {
			Common::Value<string> value = results->getValueAsString(name);
			properties << name << "=" <<
				(value.hasValue() ? value.getValue() : "no value") << " ";
		}
		return properties.str();
	}

	string propertiesOf(const char* ipAddress) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(ipAddress));
		return propertiesOf(results.get());
	}

	// Every property as the C API reads it.
	string cPropertiesOf(const char* ipAddress) {
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			ipAddress,
			strlen(ipAddress),
			exception);
		stringstream properties;
		const int count = (int)((fiftyoneDegreesDataSetIpi*)results->b.dataSet)
			->b.b.available->count;
		for (int i = 0; i < count; i++) {
			char values[256] = "";
			fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
				results, i, values, sizeof(values), "|", exception);
			properties << i << "=" << values << " has values " <<
				fiftyoneDegreesResultsIpiGetHasValues(results, i, exception) <<
				" reason " << (int)fiftyoneDegreesResultsIpiGetNoValueReason(
					results, i, exception) << " ";
		}
		fiftyoneDegreesResultsIpiFree(results);
		EXCEPTION_THROW;
		return properties.str();
	}

	// Every property and the target address.
	string answerOf(Ipi::ResultsIpi* results) {
		stringstream answer;
		answer << propertiesOf(results);
		Common::Value<Ipi::IpAddress> target = results->getTargetIpAddress();
		answer << "target type " << (int)target.getValue().getType() << ":";
		const int length = target.getValue().getType() ==
			FIFTYONE_DEGREES_IP_TYPE_IPV4 ?
			FIFTYONE_DEGREES_IPV4_LENGTH : FIFTYONE_DEGREES_IPV6_LENGTH;
		for (int i = 0; i < length; i++) {
			answer << " " << (int)target.getValue().getIpAddress()[i];
		}
		return answer.str();
	}

	string answerOf(const char* ipAddress) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(ipAddress));
		return answerOf(results.get());
	}

	string asnOf(const char* ipAddress) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(ipAddress));
		return results->getValueAsString("Asn").getValue();
	}

	void expectNoProfile(initializer_list<const char*> ipAddresses) {
		for (const char* ipAddress : ipAddresses) {
			const vector<uint32_t> offsets = rawOffsets(ipAddress);
			ASSERT_FALSE(offsets.empty()) << ipAddress;
			for (const uint32_t offset : offsets) {
				EXPECT_EQ(noProfile, offset) << ipAddress << dataHint;
			}
			EXPECT_EQ(defaultAsn, asnOf(ipAddress)) << ipAddress;
		}
	}

	void expectProfile(initializer_list<const char*> ipAddresses) {
		for (const char* ipAddress : ipAddresses) {
			const vector<uint32_t> offsets = rawOffsets(ipAddress);
			ASSERT_FALSE(offsets.empty()) << ipAddress;
			for (const uint32_t offset : offsets) {
				EXPECT_NE(noProfile, offset) << ipAddress;
			}
		}
	}

	// Each form must get exactly what the IPv4 address gets.
	void expectSameAs(
		const char* ipv4,
		initializer_list<const char*> ipAddresses) {
		for (const char* ipAddress : ipAddresses) {
			EXPECT_EQ(rawOffsets(ipv4), rawOffsets(ipAddress)) << ipAddress;
			EXPECT_EQ(answerOf(ipv4), answerOf(ipAddress)) << ipAddress;
		}
	}
};

TEST_F(IpiAddressAnswerTests, BelowLowestKeyHasNoProfile) {
	expectNoProfile({
		"0.0.0.0",
		"0.255.255.255",
		"::",
		"::1",
		"2001:4:111:ffff:ffff:ffff:ffff:ffff" });
}

// A key's own address and anything above it still reach a leaf, including ones
// whose profile holds no values, and those are not the same as no profile.
TEST_F(IpiAddressAnswerTests, AtOrAboveLowestKeyHasAProfile) {
	expectProfile({
		"1.0.0.0",
		"127.0.0.1",
		"128.0.0.0",
		"2001:4:112::",
		"2001:4:113::" });
}

// 127.0.0.1 and 2001:4:113:: reach a profile with no values. A data file can give
// the addresses below the lowest key such a profile, which must change nothing.
TEST_F(IpiAddressAnswerTests, NoProfileReadsLikeAnEmptyProfile) {
	EXPECT_EQ(defaultAsn, asnOf("127.0.0.1"));
	EXPECT_EQ(defaultAsn, asnOf("2001:4:113::"));
	EXPECT_EQ(propertiesOf("127.0.0.1"), propertiesOf("0.0.0.0"));
	EXPECT_EQ(propertiesOf("2001:4:113::"), propertiesOf("::"));
	EXPECT_EQ(cPropertiesOf("127.0.0.1"), cPropertiesOf("0.0.0.0"));
	EXPECT_EQ(cPropertiesOf("2001:4:113::"), cPropertiesOf("::"));
}

TEST_F(IpiAddressAnswerTests, LowestKeysKeepTheirAnswers) {
	EXPECT_NE(defaultAsn, asnOf("1.0.0.0")) << dataHint;
	EXPECT_NE(defaultAsn, asnOf("2001:4:112::")) << dataHint;
}

TEST_F(IpiAddressAnswerTests, MappedFormsMatchIpv4) {
	expectSameAs("1.0.0.0", {
		"::ffff:1.0.0.0",
		"::FFFF:1.0.0.0",
		"::ffff:100:0",
		"0:0:0:0:0:ffff:100:0",
		"0000:0000:0000:0000:0000:ffff:0100:0000" });
	expectSameAs("8.8.8.8", {
		"::ffff:8.8.8.8",
		"::ffff:808:808",
		"0:0:0:0:0:ffff:808:808" });
}

TEST_F(IpiAddressAnswerTests, MappedBelowLowestKeyHasNoProfile) {
	expectNoProfile({ "::ffff:0.0.0.0", "::ffff:0:0", "::ffff:0.255.255.255" });
}

TEST_F(IpiAddressAnswerTests, MappedBytesMatchIpv4) {
	unsigned char mapped[FIFTYONE_DEGREES_IPV6_LENGTH] =
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 0, 0, 0 };
	unique_ptr<Ipi::ResultsIpi> results(engine->process(
		mapped,
		sizeof(mapped),
		FIFTYONE_DEGREES_IP_TYPE_IPV6));
	EXPECT_EQ(answerOf("1.0.0.0"), answerOf(results.get()));
}

TEST_F(IpiAddressAnswerTests, MappedEvidenceMatchesIpv4) {
	Ipi::EvidenceIpi evidence;
	evidence[engine->getKeys()->at(0)] = "::ffff:1.0.0.0";
	unique_ptr<Ipi::ResultsIpi> results(engine->process(&evidence));
	EXPECT_EQ(answerOf("1.0.0.0"), answerOf(results.get()));
}

// Other addresses that embed an IPv4 address are not mapped, so stay IPv6.
TEST_F(IpiAddressAnswerTests, OtherFormsStayIpv6) {
	expectNoProfile({ "::1.0.0.0", "::ffff:0:1.0.0.0", "1::ffff:1.0.0.0" });
}

class IpiMappedAddressRangeTests : public Base {};

// The ASN file has no range properties, so this uses a file that has, which CI
// provides, to compare every property including IpRangeStart and IpRangeEnd.
TEST_F(IpiMappedAddressRangeTests, EveryPropertyMatchesIpv4) {
	string dataFilePath = "";
	for (int i = 0; i < _IpiFileNamesLength && dataFilePath == ""; i++) {
		dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
	}
	if (dataFilePath == "") {
		GTEST_SKIP() << "No data file with range properties.";
	}
	Ipi::ConfigIpi config;
	// Balanced, so the test does not hold the whole Enterprise file in memory.
	config.setBalanced();
	Common::RequiredPropertiesConfig properties;
	Ipi::EngineIpi engine(dataFilePath, &config, &properties);
	unique_ptr<Ipi::ResultsIpi> ipv4(engine.process("8.8.8.8"));
	unique_ptr<Ipi::ResultsIpi> mapped(engine.process("::ffff:8.8.8.8"));
	for (const string& name : engine.getRequiredProperties()) {
		Common::Value<string> expected = ipv4->getValueAsString(name);
		Common::Value<string> actual = mapped->getValueAsString(name);
		ASSERT_EQ(expected.hasValue(), actual.hasValue()) << name;
		if (expected.hasValue()) {
			EXPECT_EQ(expected.getValue(), actual.getValue()) << name;
		}
	}
}
