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

/**
 * @file IpiValueTextsTests.cpp
 * @brief Checks the text fiftyoneDegreesResultsIpiGetValueTexts gives for
 * each value, and that the C++ results give the same, against the text
 * written here with a stream, being the value followed by a colon and the
 * weight when the property has more than one value or the weight is not
 * whole. Every property is read for the IP addresses of the comparison with
 * the .NET packages and for a spread of other addresses.
 */

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using std::string;
using std::vector;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

/** Bytes the C function is given, as the C++ results give it. */
static const size_t valueTextsStackBytes = 128;

/** Bytes for the text of a value written here, enlarged when needed. */
static const size_t valueTextsBufferBytes = 1024;

/** Bytes that hold the text of every value of a property. */
static const size_t valueTextsLargeBytes = 1024 * 1024;

/** The buffer sizes the text must be the same for. */
static const size_t valueTextsBufferSizes[] = {
	0, 1, 17, valueTextsStackBytes, valueTextsLargeBytes };

/** One or two items of evidence. */
struct ValueTextsCase {
	const char* key;
	const char* value;
	const char* otherKey;
	const char* otherValue;
};

/**
 * The IP address evidence of the cases in 51Degrees/51degrees-rust
 * parity/corpus/evidence.json.
 */
static const ValueTextsCase valueTextsParityCases[] = {
	{ "server.client-ip", "185.28.167.77", nullptr, nullptr },
	{ "server.client-ip", "81.2.69.142", nullptr, nullptr },
	{ "server.client-ip", "102.89.32.1", nullptr, nullptr },
	{ "server.client-ip", "1.1.1.1", nullptr, nullptr },
	{ "server.client-ip", "8.8.8.8", nullptr, nullptr },
	{ "server.client-ip", "2a00:23c5:a1a6:a01::1", nullptr, nullptr },
	{ "server.client-ip", "2001:4860:4860::8888", nullptr, nullptr },
	{ "server.client-ip", "::ffff:1.1.1.1", nullptr, nullptr },
	{ "server.client-ip", "[2a00:23c5:a1a6:a01::1]", nullptr, nullptr },
	{ "server.client-ip", "127.0.0.1", nullptr, nullptr },
	{ "server.client-ip", "::1", nullptr, nullptr },
	{ "server.client-ip", "10.0.0.1", nullptr, nullptr },
	{ "server.client-ip", "192.168.1.1", nullptr, nullptr },
	{ "server.client-ip", "0.0.0.0", nullptr, nullptr },
	{ "server.client-ip", "255.255.255.255", nullptr, nullptr },
	{ "server.client-ip", "not-an-ip", nullptr, nullptr },
	{ "server.client-ip", "", nullptr, nullptr },
	{ "query.client-ip-51d", "81.2.69.142", nullptr, nullptr },
	{
		"server.client-ip", "1.1.1.1",
		"query.client-ip-51d", "81.2.69.142"
	},
};

/** IPv6 addresses in regions the IPv4 spread does not reach. */
static const char* valueTextsIpv6Addresses[] = {
	"2400:cb00::1",
	"2405:200::1",
	"2600:1f18::1",
	"2607:f8b0::1",
	"2804:14c::1",
	"2a01:4f8::1",
	"2a02:c7f::1",
	"2c0f:fb50::1",
};

/** The first and the step of the IPv4 spread. */
static const int valueTextsIpv4First = 1;
static const int valueTextsIpv4Step = 4;
static const int valueTextsIpv4Last = 223;

/** Weights below this have an exponent in the text. */
static const double valueTextsExponentBelow = 0.0001;

/** What the reads compared. */
struct ValueTextsCounts {
	uint32_t lookups = 0;
	uint32_t values = 0;
	uint32_t weighted = 0;
	uint32_t exponents = 0;
	uint32_t cppValues = 0;
};

/**
 * The text of each value of the property, written as the C++ wrote it with a
 * stream from the text of the stored value. Returns false when the results
 * give no values for the property.
 */
static bool valueTextsStreamed(
	fiftyoneDegreesResultsIpi* results,
	const int index,
	vector<string>& texts,
	ValueTextsCounts& counts) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const fiftyoneDegreesDataSetIpi* dataSet =
		(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
	const int propertyIndex =
		fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
			dataSet->b.b.available,
			index);
	const fiftyoneDegreesWeightedItem* items =
		fiftyoneDegreesResultsIpiGetValues(results, index, exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	if (items == nullptr) {
		return false;
	}
	const fiftyoneDegreesPropertyValueType type =
		fiftyoneDegreesPropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			(uint32_t)propertyIndex,
			exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	const uint32_t count = results->values.count;
	vector<char> buffer(valueTextsBufferBytes);
	for (uint32_t i = 0; i < count; i++) {
		// Writes the text of the value, returning the bytes it needs when
		// the buffer is too small.
		const auto write = [&]() {
			fiftyoneDegreesStringBuilder builder = {
				buffer.data(),
				buffer.size() };
			fiftyoneDegreesStringBuilderInit(&builder);
			fiftyoneDegreesStringBuilderAddStringValue(
				&builder,
				(const fiftyoneDegreesStoredBinaryValue*)
					items[i].item.data.ptr,
				type,
				fiftyoneDegreesDefaultWktDecimalPlaces,
				exception);
			fiftyoneDegreesStringBuilderComplete(&builder);
			return builder.full ? builder.added + 1 : (size_t)0;
		};
		const size_t needed = write();
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		if (needed > 0) {
			buffer.resize(needed);
			write();
			FIFTYONE_DEGREES_EXCEPTION_THROW;
		}
		std::stringstream stream;
		stream << buffer.data();
		const uint32_t raw = items[i].rawWeighting;
		if (count > 1 || raw != FIFTYONE_DEGREES_WEIGHTED_ITEM_MAX_WEIGHT) {
			const double weight = static_cast<double>(raw) /
				static_cast<double>(FIFTYONE_DEGREES_WEIGHTED_ITEM_MAX_WEIGHT);
			stream << ":" << weight;
			counts.weighted++;
			if (weight > 0 && weight < valueTextsExponentBelow) {
				counts.exponents++;
			}
		}
		texts.push_back(stream.str());
		counts.values++;
	}
	return true;
}

/**
 * The text of each value through the C function, given a buffer of the bytes
 * asked for, or no buffer for none. Returns false when the C function gives
 * no text, and sets inBuffer when the text was written into the buffer.
 */
static bool valueTextsFromC(
	fiftyoneDegreesResultsIpi* results,
	const int index,
	const size_t bufferBytes,
	vector<string>& texts,
	bool& inBuffer) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	vector<char> buffer(bufferBytes);
	char* const given = bufferBytes > 0 ? buffer.data() : nullptr;
	uint32_t count = UINT32_MAX;
	char* const text = fiftyoneDegreesResultsIpiGetValueTexts(
		results,
		index,
		given,
		bufferBytes,
		&count,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	inBuffer = text != nullptr && text == given;
	if (text == nullptr) {
		EXPECT_EQ(0u, count) << "No text should have no values.";
		return false;
	}
	const char* next = text;
	for (uint32_t i = 0; i < count; i++) {
		texts.push_back(string(next));
		next += texts.back().size() + 1;
	}
	if (inBuffer == false) {
		fiftyoneDegreesFree(text);
	}
	return true;
}

class IpiValueTextsTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0; i < _IpiFileNamesLength && dataFilePath.empty(); i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No data file was found.";
		}
		fiftyoneDegreesConfigIpi cConfig = fiftyoneDegreesIpiLowMemoryConfig;
		fiftyoneDegreesPropertiesRequired cProperties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&cConfig,
				&cProperties,
				dataFilePath.c_str(),
				exception);
		ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		loaded = true;
		config.setLowMemory();
		engine = std::make_unique<Ipi::EngineIpi>(
			dataFilePath,
			&config,
			&properties);
	}

	void TearDown() override {
		engine.reset();
		if (loaded) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		string().swap(dataFilePath);
		Base::TearDown();
	}

protected:
	string dataFilePath;
	fiftyoneDegreesResourceManager manager;
	bool loaded = false;
	Ipi::ConfigIpi config;
	Common::RequiredPropertiesConfig properties;
	std::unique_ptr<Ipi::EngineIpi> engine;

	static void addEvidence(
		Ipi::EvidenceIpi& evidence,
		const ValueTextsCase& item) {
		evidence[item.key] = item.value;
		if (item.otherKey != nullptr) {
			evidence[item.otherKey] = item.otherValue;
		}
	}

	/** C results for the evidence, which the caller frees. */
	fiftyoneDegreesResultsIpi* lookUp(const ValueTextsCase& item) {
		Ipi::EvidenceIpi evidence;
		addEvidence(evidence, item);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpiFromEvidence(
			results,
			evidence.get(),
			exception);
		// The C++ engine reads an address it cannot parse as no address.
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY ||
			FIFTYONE_DEGREES_EXCEPTION_CHECK(
				FIFTYONE_DEGREES_STATUS_INCORRECT_IP_ADDRESS_FORMAT)) <<
			item.value;
		return results;
	}

	static int requiredCount(fiftyoneDegreesResultsIpi* results) {
		return (int)fiftyoneDegreesPropertiesGetRequiredPropertyCount(
			((const fiftyoneDegreesDataSetIpi*)results->b.dataSet)
				->b.b.available);
	}

	static string nameOf(fiftyoneDegreesResultsIpi* results, const int index) {
		return string(FIFTYONE_DEGREES_STRING(
			fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
				((const fiftyoneDegreesDataSetIpi*)results->b.dataSet)
					->b.b.available,
				index)));
	}

	/**
	 * Reads every property for the evidence through the C function and the
	 * C++ results, and expects both to give the text written here.
	 */
	void expectStreamForm(
		const ValueTextsCase& item,
		ValueTextsCounts& counts) {
		fiftyoneDegreesResultsIpi* results = lookUp(item);
		Ipi::EvidenceIpi evidence;
		addEvidence(evidence, item);
		std::unique_ptr<Ipi::ResultsIpi> cppResults(engine->process(&evidence));
		counts.lookups++;
		const int count = requiredCount(results);
		for (int i = 0; i < count; i++) {
			vector<string> expected;
			const bool found = valueTextsStreamed(results, i, expected, counts);
			vector<string> actual;
			bool inBuffer = false;
			EXPECT_EQ(found, valueTextsFromC(
				results,
				i,
				valueTextsStackBytes,
				actual,
				inBuffer)) << item.value << " " << nameOf(results, i);
			EXPECT_EQ(expected, actual) << item.value << " " <<
				nameOf(results, i);
			Common::Value<vector<string>> values = cppResults->getValues(i);
			if (values.hasValue()) {
				EXPECT_EQ(expected, values.getValue()) << item.value << " " <<
					nameOf(results, i);
				counts.cppValues += (uint32_t)values.getValue().size();
			}
		}
		fiftyoneDegreesResultsIpiFree(results);
	}
};

TEST_F(IpiValueTextsTests, EveryValueHasTheStreamForm) {
	ValueTextsCounts counts;
	for (const ValueTextsCase& item : valueTextsParityCases) {
		expectStreamForm(item, counts);
	}
	for (int a = valueTextsIpv4First; a <= valueTextsIpv4Last;
		a += valueTextsIpv4Step) {
		std::stringstream address;
		address << a << "." << (a * 37) % 256 << "." << (a * 101) % 256 <<
			".1";
		const string text = address.str();
		const ValueTextsCase item = {
			"server.client-ip", text.c_str(), nullptr, nullptr };
		expectStreamForm(item, counts);
	}
	for (const char* address : valueTextsIpv6Addresses) {
		const ValueTextsCase item = {
			"server.client-ip", address, nullptr, nullptr };
		expectStreamForm(item, counts);
	}
	EXPECT_GT(counts.values, 0u) << "Some property should have a value.";
	EXPECT_GT(counts.cppValues, 0u) << "The C++ should give some values.";
	std::cout << "Compared " << counts.values << " values from " <<
		counts.lookups << " lookups, " << counts.weighted <<
		" with a weight and " << counts.exponents <<
		" of those with an exponent.\n";
}

TEST_F(IpiValueTextsTests, TheTextIsTheSameInEveryBuffer) {
	uint32_t inLarge = 0;
	for (const ValueTextsCase& item : valueTextsParityCases) {
		fiftyoneDegreesResultsIpi* results = lookUp(item);
		const int count = requiredCount(results);
		for (int i = 0; i < count; i++) {
			vector<string> first;
			bool inBuffer = false;
			const bool found = valueTextsFromC(
				results,
				i,
				valueTextsBufferSizes[0],
				first,
				inBuffer);
			EXPECT_FALSE(inBuffer) << "With no buffer the text is allocated.";
			for (const size_t bytes : valueTextsBufferSizes) {
				vector<string> texts;
				EXPECT_EQ(found, valueTextsFromC(
					results,
					i,
					bytes,
					texts,
					inBuffer)) << item.value << " " << nameOf(results, i);
				EXPECT_EQ(first, texts) << item.value << " " <<
					nameOf(results, i) << " in " << bytes << " bytes";
				if (bytes == valueTextsLargeBytes && inBuffer) {
					inLarge++;
				}
			}
		}
		fiftyoneDegreesResultsIpiFree(results);
	}
	EXPECT_GT(inLarge, 0u) << "Some text should be written into the buffer.";
}

TEST_F(IpiValueTextsTests, NoTextForAPropertyThatIsNotRequired) {
	fiftyoneDegreesResultsIpi* results = lookUp(valueTextsParityCases[0]);
	const int outside[] = { -1, requiredCount(results) };
	for (const int index : outside) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		char buffer[valueTextsStackBytes];
		uint32_t count = UINT32_MAX;
		EXPECT_EQ(nullptr, fiftyoneDegreesResultsIpiGetValueTexts(
			results,
			index,
			buffer,
			sizeof(buffer),
			&count,
			exception)) << index;
		EXPECT_EQ(0u, count) << index;
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << index;
	}
	fiftyoneDegreesResultsIpiFree(results);
}
