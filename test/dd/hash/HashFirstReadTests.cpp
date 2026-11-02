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
 * @file HashFirstReadTests.cpp
 * @brief Tests that Hash results walk a component's graph the first time one
 * of its properties is read, and that every answer is the one given when
 * every graph is walked first, whatever is read first. The evidence covers
 * property overrides, device ids, special evidence, profile overrides,
 * default profiles and the high entropy values override. The walks are
 * counted by wrapping the method the results call to walk a component.
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/ResultsHashSerializer.hpp"
#include "../../../src/dd/hash/hash.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

const char iPhoneUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53";

const char chromeUserAgent[] =
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36";

/** High entropy values for Chrome 126 on macOS 14.5, encoded in base 64. */
const char highEntropyValues[] =
	"eyJicmFuZHMiOlt7ImJyYW5kIjoiTm90L0EpQnJhbmQiLCJ2ZXJzaW9uIjoiOCJ9LHsiYnJh"
	"bmQiOiJDaHJvbWl1bSIsInZlcnNpb24iOiIxMjYifSx7ImJyYW5kIjoiR29vZ2xlIENocm9t"
	"ZSIsInZlcnNpb24iOiIxMjYifV0sImZ1bGxWZXJzaW9uTGlzdCI6W3siYnJhbmQiOiJOb3Qv"
	"QSlCcmFuZCIsInZlcnNpb24iOiI4LjAuMC4wIn0seyJicmFuZCI6IkNocm9taXVtIiwidmVy"
	"c2lvbiI6IjEyNi4wLjY0NzguMTI3In0seyJicmFuZCI6Ikdvb2dsZSBDaHJvbWUiLCJ2ZXJz"
	"aW9uIjoiMTI2LjAuNjQ3OC4xMjcifV0sIm1vYmlsZSI6ZmFsc2UsIm1vZGVsIjoiIiwicGxh"
	"dGZvcm0iOiJtYWNPUyIsInBsYXRmb3JtVmVyc2lvbiI6IjE0LjUuMCJ9";

/** A structured User-Agent for Chrome 124 on Windows 11. */
const char structuredUserAgent[] =
	"{\"browsers\":[{\"brand\":\"Chromium\",\"version\":[\"124\",\"0\","
	"\"6367\",\"91\"]},{\"brand\":\"Google Chrome\",\"version\":[\"124\","
	"\"0\",\"6367\",\"91\"]},{\"brand\":\"Not-A.Brand\",\"version\":[\"99\","
	"\"0\",\"0\",\"0\"]}],\"platform\":{\"brand\":\"Windows\",\"version\":"
	"[\"14\",\"0\",\"0\"]},\"mobile\":0,\"architecture\":\"x86\","
	"\"source\":2}";

/** The overridable property the override tests use, and its value. */
const char overriddenName[] = "ScreenPixelsWidth";
const char overriddenValue[] = "12345";

/**
 * Properties of each component, a property evidence can override with the
 * property whose name starts with its name and whose JavaScript measures it,
 * as evidence overrides a property only when that property is named too, and
 * one whose value depends on the high entropy values, for the engines that
 * name their properties.
 */
const std::vector<std::string> namedProperties = {
	"IsMobile",
	"HardwareName",
	"PlatformName",
	"BrowserName",
	"IsCrawler",
	overriddenName,
	std::string(overriddenName) + "Javascript",
	"JavascriptGetHighEntropyValues" };

/** Characters shown either side of the first difference between texts. */
const size_t differenceContextCharacters = 80;

/** Bytes for the text of a property's values or the JSON. */
const size_t textBufferBytes = 1 << 20;

/** The components walked, in order, by the results that count their walks. */
std::vector<int>* walkLog = nullptr;

/** The walk the results make when they are not counting, or null. */
fiftyoneDegreesResultsEvaluateMethod walkMethod = nullptr;

/** Records the component in the log, then walks it if the results walk. */
void countWalk(
	fiftyoneDegreesResultsBase* results,
	unsigned char componentIndex,
	fiftyoneDegreesException* exception) {
	if (walkLog != nullptr) {
		walkLog->push_back((int)componentIndex);
	}
	if (walkMethod != nullptr) {
		walkMethod(results, componentIndex, exception);
	}
}

/** Makes the results record each walk in the log given. */
void countWalks(fiftyoneDegreesResultsHash* results, std::vector<int>& log) {
	walkMethod = results->b.b.evaluate;
	walkLog = &log;
	results->b.b.evaluate = countWalk;
}

/** Where two texts first differ, or nothing when they are the same. */
std::string firstDifference(
	const std::string& expected,
	const std::string& actual) {
	if (expected == actual) {
		return std::string();
	}
	size_t same = 0;
	while (same < expected.size() && same < actual.size() &&
		expected[same] == actual[same]) {
		same++;
	}
	const size_t from = same > differenceContextCharacters ?
		same - differenceContextCharacters : 0;
	return "differs from character " + std::to_string(same) +
		", expected '" + expected.substr(from, 2 * differenceContextCharacters) +
		"', found '" + actual.substr(from, 2 * differenceContextCharacters) + "'";
}

/** The evidence of one case, as the keys and values the engine is given. */
typedef std::vector<std::pair<std::string, std::string>> EvidenceList;

/** The values of every property, read by name in the order given. */
std::string readProperties(
	ResultsHash* results,
	const std::vector<std::string>& names) {
	std::map<std::string, std::string> read;
	for (const std::string& name : names) {
		Value<std::vector<std::string>> values = results->getValues(name);
		std::string text;
		if (values.hasValue()) {
			for (const std::string& value : values.getValue()) {
				text.append(value).append("|");
			}
		}
		else {
			const char* message = values.getNoValueMessage();
			text = "no value " + std::to_string(
				(int)values.getNoValueReason()) + " " +
				(message == nullptr ? "" : message);
		}
		read[name] = text;
	}
	std::string text;
	for (const auto& entry : read) {
		text.append(entry.first).append("=").append(entry.second).append("\n");
	}
	return text;
}

/** What the readers of the result items give. */
std::string readItems(ResultsHash* results) {
	std::ostringstream text;
	text << "deviceId=" << results->getDeviceId() <<
		"\niterations=" << results->getIterations() <<
		"\nmatchedNodes=" << results->getMatchedNodes() <<
		"\ndrift=" << results->getDrift() <<
		"\ndifference=" << results->getDifference() <<
		"\nmethod=" << results->getMethod() <<
		"\nuserAgents=" << results->getUserAgents() << "\n";
	for (int i = 0; i < results->getUserAgents(); i++) {
		text << i << ": " << results->getDeviceId((uint32_t)i) <<
			" drift=" << results->getDrift((uint32_t)i) <<
			" difference=" << results->getDifference((uint32_t)i) <<
			" method=" << results->getMethod((uint32_t)i) <<
			" userAgent=" << results->getUserAgent(i) << "\n";
	}
	return text.str();
}

/** The JSON of every property. */
std::string readJson(ResultsHash* results) {
	ResultsHashSerializer serializer;
	return "json=" + serializer.allValuesJson(results) + "\n";
}

/** The orders in which the readers are called. */
enum class ReadOrder {
	PropertiesFirst,
	PropertiesInReverseFirst,
	ItemsFirst,
	JsonFirst
};

/** Everything the results give, read in the order given. */
std::string readEverything(ResultsHash* results, ReadOrder order) {
	std::vector<std::string> names = results->getProperties();
	std::string properties, items, json;
	switch (order) {
	case ReadOrder::PropertiesFirst:
		properties = readProperties(results, names);
		items = readItems(results);
		json = readJson(results);
		break;
	case ReadOrder::PropertiesInReverseFirst:
		std::reverse(names.begin(), names.end());
		properties = readProperties(results, names);
		json = readJson(results);
		items = readItems(results);
		break;
	case ReadOrder::ItemsFirst:
		items = readItems(results);
		properties = readProperties(results, names);
		json = readJson(results);
		break;
	case ReadOrder::JsonFirst:
		json = readJson(results);
		items = readItems(results);
		properties = readProperties(results, names);
		break;
	}
	return properties + items + json;
}

/** The value of the property read through the C API, or why there is none. */
std::string valueOf(
	fiftyoneDegreesResultsHash* results,
	const char* name) {
	std::vector<char> buffer(textBufferBytes, '\0');
	char separator[] = "|";
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashGetValuesString(
		results,
		name,
		buffer.data(),
		buffer.size(),
		separator,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(buffer.data());
}

/** The JSON of every property read through the C API. */
std::string jsonOf(fiftyoneDegreesResultsHash* results) {
	std::vector<char> buffer(textBufferBytes, '\0');
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashGetValuesJson(
		results,
		buffer.data(),
		buffer.size(),
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(buffer.data());
}

fiftyoneDegreesDataSetHash* dataSetOf(fiftyoneDegreesResultsHash* results) {
	return (fiftyoneDegreesDataSetHash*)results->b.b.dataSet;
}

/** The number of required property indexes of the results' data set. */
int positionCount(fiftyoneDegreesResultsHash* results) {
	return (int)fiftyoneDegreesPropertiesGetRequiredPropertyCount(
		dataSetOf(results)->b.b.available);
}

/** The required property index of the property named, or -1. */
int positionOf(fiftyoneDegreesResultsHash* results, const char* name) {
	fiftyoneDegreesPropertiesAvailable* available =
		dataSetOf(results)->b.b.available;
	for (int i = 0; i < positionCount(results); i++) {
		const char* named = FIFTYONE_DEGREES_STRING(
			fiftyoneDegreesPropertiesGetNameFromRequiredIndex(available, i));
		if (named != nullptr && strcmp(name, named) == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * The index in the available properties of the property at the required
 * property index, or -1 if the data set does not have it.
 */
int availableIndexOf(fiftyoneDegreesResultsHash* results, int position) {
	return fiftyoneDegreesPropertiesGetAvailableIndex(
		dataSetOf(results)->b.b.available,
		position);
}

/** The component of the property at the required property index. */
int componentOf(fiftyoneDegreesResultsHash* results, int position) {
	return (int)dataSetOf(results)->b.b.available->items[
		availableIndexOf(results, position)].componentIndex;
}

/**
 * The required property index of the first property, other than the one at
 * the index given, on the same component, or -1 if there is none.
 */
int otherOnComponent(fiftyoneDegreesResultsHash* results, int position) {
	for (int i = 0; i < positionCount(results); i++) {
		if (i != position &&
			availableIndexOf(results, i) >= 0 &&
			componentOf(results, i) == componentOf(results, position)) {
			return i;
		}
	}
	return -1;
}

/**
 * True if the property at the required property index can take its value
 * from the evidence.
 */
bool isOverridable(fiftyoneDegreesResultsHash* results, int position) {
	fiftyoneDegreesOverridePropertyArray* overridable =
		dataSetOf(results)->b.b.overridable;
	const int index = availableIndexOf(results, position);
	for (uint32_t i = 0; overridable != nullptr && i < overridable->count; i++) {
		if ((int)overridable->items[i].requiredPropertyIndex == index) {
			return true;
		}
	}
	return false;
}

}

/**
 * Tests through the C API on the first Hash data file found, with every
 * property available. A skip in SetUp does not stop the test with the Visual
 * Studio googletest, so each test first checks that SetUp created the
 * manager.
 */
class HashFirstReadCTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _HashFileNamesLength && dataFilePath.empty();
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No Hash data file was found.";
		}
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				dataFilePath.c_str(),
				exception));
		managerReady = true;
	}

	void TearDown() override {
		for (fiftyoneDegreesEvidenceKeyValuePairArray* e : evidences) {
			fiftyoneDegreesEvidenceFree(e);
		}
		std::vector<fiftyoneDegreesEvidenceKeyValuePairArray*>().swap(
			evidences);
		if (managerReady) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		walkLog = nullptr;
		walkMethod = nullptr;
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(dataFilePath);
		Base::TearDown();
	}

protected:
	std::string dataFilePath;
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashBalancedConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;
	std::vector<fiftyoneDegreesEvidenceKeyValuePairArray*> evidences;

	/** Results that can hold every override value of the data set. */
	fiftyoneDegreesResultsHash* create() {
		return fiftyoneDegreesResultsHashCreate(&manager, 0, 1);
	}

	/** Evidence with the pairs given, freed when the test ends. */
	fiftyoneDegreesEvidenceKeyValuePairArray* evidenceOf(
		const std::vector<std::pair<
			fiftyoneDegreesEvidencePrefix,
			std::pair<const char*, const char*>>>& pairs) {
		fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
			fiftyoneDegreesEvidenceCreate((uint32_t)pairs.size());
		for (const auto& pair : pairs) {
			fiftyoneDegreesEvidenceAddString(
				evidence,
				pair.first,
				pair.second.first,
				pair.second.second);
		}
		evidences.push_back(evidence);
		return evidence;
	}

	void populate(
		fiftyoneDegreesResultsHash* results,
		fiftyoneDegreesEvidenceKeyValuePairArray* evidence) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromEvidence(results, evidence, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}

	static void walkEveryGraph(fiftyoneDegreesResultsHash* results) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&results->b.b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}
};

/**
 * Populating the results walks no graph. A read walks the graph of the
 * property's component once, and a read of a property on another component
 * walks that component.
 */
TEST_F(HashFirstReadCTests, Read_WalksOnlyTheComponentRead) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* results = create();
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", iPhoneUserAgent } } }));
	EXPECT_TRUE(walks.empty()) << "Populating the results walked a graph.";

	const int platform = positionOf(results, "PlatformName");
	const int browser = positionOf(results, "BrowserName");
	ASSERT_GE(platform, 0);
	ASSERT_GE(browser, 0);
	valueOf(results, "PlatformName");
	EXPECT_EQ(std::vector<int>({ componentOf(results, platform) }), walks);
	valueOf(results, "PlatformName");
	EXPECT_EQ(std::vector<int>({ componentOf(results, platform) }), walks) <<
		"A second read of the property walked its graph again.";
	valueOf(results, "BrowserName");
	EXPECT_EQ(
		std::vector<int>({
			componentOf(results, platform),
			componentOf(results, browser) }),
		walks);

	populate(results, evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", chromeUserAgent } } }));
	walks.clear();
	valueOf(results, "PlatformName");
	EXPECT_EQ(std::vector<int>({ componentOf(results, platform) }), walks) <<
		"Populating the results again did not make the component wait for "
		"its first read.";
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * A property overridden by the evidence reads its override without walking
 * its component, while another property of the same component walks it. The
 * has values and no value reason reads agree, and the JSON carries the
 * override, as it does when every graph is walked first.
 */
TEST_F(HashFirstReadCTests, Override_ReadsWithoutWalking) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* results = create();
	const int overridden = positionOf(results, overriddenName);
	if (overridden < 0 || isOverridable(results, overridden) == false) {
		fiftyoneDegreesResultsHashFree(results);
		GTEST_SKIP() << "The data file cannot override " << overriddenName;
	}
	const int other = otherOnComponent(results, overridden);
	ASSERT_GE(other, 0);
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence = evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", iPhoneUserAgent } },
		{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
			{ "51D_ScreenPixelsWidth", overriddenValue } } });

	fiftyoneDegreesResultsHash* walkedFirst = create();
	populate(walkedFirst, evidence);
	walkEveryGraph(walkedFirst);

	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, evidence);
	EXPECT_EQ(std::string(overriddenValue), valueOf(results, overriddenName));
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	EXPECT_TRUE(fiftyoneDegreesResultsHashGetHasValues(
		results,
		overridden,
		exception));
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	EXPECT_TRUE(walks.empty()) <<
		"Reading the overridden property walked its component.";

	const char* otherName = FIFTYONE_DEGREES_STRING(
		fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
			dataSetOf(results)->b.b.available,
			other));
	EXPECT_EQ(valueOf(walkedFirst, otherName), valueOf(results, otherName));
	EXPECT_EQ(
		fiftyoneDegreesResultsHashGetHasValues(walkedFirst, other, exception),
		fiftyoneDegreesResultsHashGetHasValues(results, other, exception));
	EXPECT_EQ(
		fiftyoneDegreesResultsHashGetNoValueReason(
			walkedFirst,
			other,
			exception),
		fiftyoneDegreesResultsHashGetNoValueReason(results, other, exception));
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	EXPECT_EQ(std::vector<int>({ componentOf(results, overridden) }), walks) <<
		"Reading another property of the component did not walk it once.";

	const std::string json = jsonOf(results);
	EXPECT_EQ(std::string(), firstDifference(jsonOf(walkedFirst), json));
	EXPECT_NE(std::string::npos, json.find(overriddenValue)) <<
		"The JSON does not carry the override.";
	fiftyoneDegreesResultsHashFree(walkedFirst);
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * A device id in the evidence sets the profile of every component, so
 * reading every property walks nothing.
 */
TEST_F(HashFirstReadCTests, DeviceId_LeavesNothingToWalk) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* first = create();
	populate(first, evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", iPhoneUserAgent } } }));
	walkEveryGraph(first);
	char deviceId[256] = "";
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesHashGetDeviceIdFromResults(
		first,
		deviceId,
		sizeof(deviceId),
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	fiftyoneDegreesResultsHashFree(first);

	fiftyoneDegreesResultsHash* results = create();
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_QUERY, { "51D_deviceId", deviceId } } }));
	const std::string json = jsonOf(results);
	EXPECT_FALSE(json.empty());
	walkEveryGraph(results);
	EXPECT_TRUE(walks.empty()) <<
		"Results populated from a device id walked a graph.";
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * The evidence can be freed before the first read, as the results keep what
 * they need of it, including the headers special evidence becomes.
 */
TEST_F(HashFirstReadCTests, Evidence_FreedBeforeTheFirstRead) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::vector<std::vector<std::pair<
		fiftyoneDegreesEvidencePrefix,
		std::pair<const char*, const char*>>>> cases = {
		{ { FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", iPhoneUserAgent } } },
		{ { FIFTYONE_DEGREES_EVIDENCE_QUERY,
			{ "51D_gethighentropyvalues", highEntropyValues } } },
		{ { FIFTYONE_DEGREES_EVIDENCE_QUERY,
			{ "51D_structureduseragent", structuredUserAgent } } } };
	for (const auto& pairs : cases) {
		fiftyoneDegreesResultsHash* walkedFirst = create();
		populate(walkedFirst, evidenceOf(pairs));
		walkEveryGraph(walkedFirst);
		const std::string expected = jsonOf(walkedFirst);
		fiftyoneDegreesResultsHashFree(walkedFirst);

		// Evidence of its own, with copies of the values, freed and wiped
		// before anything is read.
		std::vector<std::string> keys, values;
		for (const auto& pair : pairs) {
			keys.push_back(pair.second.first);
			values.push_back(pair.second.second);
		}
		fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
			fiftyoneDegreesEvidenceCreate((uint32_t)pairs.size());
		for (size_t i = 0; i < pairs.size(); i++) {
			fiftyoneDegreesEvidenceAddString(
				evidence,
				pairs[i].first,
				keys[i].c_str(),
				values[i].c_str());
		}
		fiftyoneDegreesResultsHash* results = create();
		populate(results, evidence);
		fiftyoneDegreesEvidenceFree(evidence);
		for (std::string& value : values) {
			std::fill(value.begin(), value.end(), 'x');
		}
		EXPECT_EQ(std::string(), firstDifference(expected, jsonOf(results)));
		fiftyoneDegreesResultsHashFree(results);
	}
}

/**
 * A profile id override replaces the profile of its component. Reading a
 * property of that component walks nothing, and walking every graph later
 * keeps the override. IsMobile is the hardware property both the Lite and
 * the enterprise data files have.
 */
TEST_F(HashFirstReadCTests, ProfileOverride_SurvivesWalkingEveryGraph) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* first = create();
	populate(first, evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", iPhoneUserAgent } } }));
	walkEveryGraph(first);
	char deviceId[256] = "";
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesHashGetDeviceIdFromResults(
		first,
		deviceId,
		sizeof(deviceId),
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	const std::string iPhoneIsMobile = valueOf(first, "IsMobile");
	const int isMobilePosition = positionOf(first, "IsMobile");
	ASSERT_GE(isMobilePosition, 0);
	const int hardwareComponent = componentOf(first, isMobilePosition);
	fiftyoneDegreesResultsHashFree(first);

	// The device id holds a profile id for each component, in component
	// order, separated by '-'.
	std::vector<std::string> profileIds;
	std::stringstream ids(deviceId);
	std::string id;
	while (std::getline(ids, id, '-')) {
		profileIds.push_back(id);
	}
	ASSERT_GT(profileIds.size(), (size_t)hardwareComponent);
	const std::string profileId = profileIds[(size_t)hardwareComponent];
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence = evidenceOf({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", chromeUserAgent } },
		{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
			{ "51D_ProfileIds", profileId.c_str() } } });

	fiftyoneDegreesResultsHash* walkedFirst = create();
	populate(walkedFirst, evidence);
	walkEveryGraph(walkedFirst);
	const std::string expected = jsonOf(walkedFirst);

	fiftyoneDegreesResultsHash* results = create();
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, evidence);
	EXPECT_EQ(valueOf(walkedFirst, "IsMobile"),
		valueOf(results, "IsMobile"));
	EXPECT_EQ(iPhoneIsMobile, valueOf(results, "IsMobile")) <<
		"The profile override was not applied.";
	EXPECT_EQ(walks.end(), std::find(
		walks.begin(),
		walks.end(),
		hardwareComponent)) <<
		"Reading a property of the overridden component walked it.";
	walkEveryGraph(results);
	EXPECT_EQ(iPhoneIsMobile, valueOf(results, "IsMobile")) <<
		"Walking every graph replaced the profile override.";
	EXPECT_EQ(std::string(), firstDifference(expected, jsonOf(results)));
	fiftyoneDegreesResultsHashFree(walkedFirst);
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * Tests through the C++ classes, comparing everything the results give when
 * read in several orders with what they give when every graph is walked
 * first, for each kind of evidence, with and without unmatched results. A
 * skip in SetUp does not stop the test with the Visual Studio googletest, so
 * each test first checks that SetUp found a data file.
 */
class HashFirstReadTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _HashFileNamesLength && dataFilePath.empty();
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No Hash data file was found.";
		}
	}

	void TearDown() override {
		std::string().swap(dataFilePath);
		Base::TearDown();
	}

protected:
	std::string dataFilePath;

	/** Walks every graph of the results, as reading everything would. */
	static void walkEveryGraph(ResultsHash* results) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&results->results->b.b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}

	/** Compares every read order with walking every graph first. */
	void compare(
		const EngineHash& engine,
		const std::string& name,
		const EvidenceList& list) {
		EvidenceDeviceDetection evidence;
		for (const auto& pair : list) {
			evidence[pair.first] = pair.second;
		}
		std::unique_ptr<ResultsHash> walked(engine.process(&evidence));
		walkEveryGraph(walked.get());
		const std::string expected =
			readEverything(walked.get(), ReadOrder::PropertiesFirst);
		for (const ReadOrder order : {
			ReadOrder::PropertiesFirst,
			ReadOrder::PropertiesInReverseFirst,
			ReadOrder::ItemsFirst,
			ReadOrder::JsonFirst }) {
			std::unique_ptr<ResultsHash> results(engine.process(&evidence));
			EXPECT_EQ(std::string(), firstDifference(
				expected,
				readEverything(results.get(), order))) <<
				name << ", read order " << (int)order;
		}

		// The evidence the results came from can go before the first read.
		EvidenceDeviceDetection* owned = new EvidenceDeviceDetection();
		for (const auto& pair : list) {
			(*owned)[pair.first] = pair.second;
		}
		std::unique_ptr<ResultsHash> freed(engine.process(owned));
		delete owned;
		EXPECT_EQ(std::string(), firstDifference(
			expected,
			readEverything(freed.get(), ReadOrder::PropertiesFirst))) <<
			name << ", evidence freed before the first read";
	}

	/**
	 * Each kind of evidence, by name, using the device id and profile id the
	 * engine gives the iPhone.
	 */
	static std::vector<std::pair<std::string, EvidenceList>> everyCase(
		const EngineHash& engine) {
		std::string deviceId, profileId;
		{
			std::unique_ptr<ResultsHash> first(engine.process(iPhoneUserAgent));
			deviceId = first->getDeviceId();
			profileId = deviceId.substr(0, deviceId.find('-'));
		}
		return {
			{ "iPhone", { { "header.User-Agent", iPhoneUserAgent } } },
			{ "Chrome", { { "header.User-Agent", chromeUserAgent } } },
			{ "client hints", {
				{ "header.User-Agent", chromeUserAgent },
				{ "header.Sec-CH-UA",
					"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" },
				{ "header.Sec-CH-UA-Mobile", "?0" },
				{ "header.Sec-CH-UA-Platform", "\"Windows\"" },
				{ "header.Sec-CH-UA-Platform-Version", "\"14.0.0\"" } } },
			{ "browser client hint only", {
				{ "header.Sec-CH-UA",
					"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" } } },
			{ "high entropy values", {
				{ "query.51D_gethighentropyvalues", highEntropyValues } } },
			{ "structured User-Agent", {
				{ "query.51D_structureduseragent", structuredUserAgent } } },
			{ "device id", { { "query.51D_deviceId", deviceId } } },
			{ "profile override", {
				{ "header.User-Agent", chromeUserAgent },
				{ "query.51D_ProfileIds", profileId } } },
			{ "profile override alone", {
				{ "query.51D_ProfileIds", profileId } } },
			{ "property override", {
				{ "header.User-Agent", iPhoneUserAgent },
				{ std::string("query.51D_") + overriddenName,
					overriddenValue } } },
			{ "property override alone", {
				{ std::string("query.51D_") + overriddenName,
					overriddenValue } } },
			{ "unknown User-Agent", { { "header.User-Agent", "unknown" } } },
			{ "no evidence", { } } };
	}

	/**
	 * Compares each kind of evidence with the configuration given, with the
	 * properties named, or with none named when there are no names.
	 */
	void compareEveryCase(
		ConfigHash& config,
		const std::vector<std::string>* names = nullptr) {
		std::unique_ptr<RequiredPropertiesConfig> properties(
			names == nullptr ?
				new RequiredPropertiesConfig() :
				new RequiredPropertiesConfig(names));
		EngineHash engine(dataFilePath.c_str(), &config, properties.get());
		for (const auto& c : everyCase(engine)) {
			compare(engine, c.first, c.second);
		}
	}

	/**
	 * Compares the values of the properties named, read in order and in
	 * reverse from an engine created naming them, with the values an engine
	 * created naming none gives after walking every graph, for each kind of
	 * evidence.
	 */
	void compareNamedWithNoneNamed(
		ConfigHash& config,
		const std::vector<std::string>& names) {
		RequiredPropertiesConfig all;
		EngineHash noneNamed(dataFilePath.c_str(), &config, &all);
		RequiredPropertiesConfig some(&names);
		EngineHash named(dataFilePath.c_str(), &config, &some);
		for (const auto& c : everyCase(noneNamed)) {
			EvidenceDeviceDetection evidence;
			for (const auto& pair : c.second) {
				evidence[pair.first] = pair.second;
			}
			std::unique_ptr<ResultsHash> walked(noneNamed.process(&evidence));
			walkEveryGraph(walked.get());
			const std::string expected = readProperties(walked.get(), names);
			for (const bool reverse : { false, true }) {
				std::vector<std::string> order = names;
				if (reverse) {
					std::reverse(order.begin(), order.end());
				}
				std::unique_ptr<ResultsHash> results(named.process(&evidence));
				EXPECT_EQ(std::string(), firstDifference(
					expected,
					readProperties(results.get(), order))) << c.first <<
					(reverse ? ", read in reverse" : "");
			}
		}
	}
};

/**
 * Every answer, read in any order, is the answer when every graph is walked
 * first, for each kind of evidence.
 */
TEST_F(HashFirstReadTests, EveryCase_MatchesWalkingEveryGraphFirst) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	compareEveryCase(config);
}

/**
 * The same with unmatched results allowed, where a component with no match
 * takes its default profile.
 */
TEST_F(HashFirstReadTests, EveryCase_AllowUnmatched_MatchesWalkingEveryGraphFirst) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	config.setAllowUnmatched(true);
	compareEveryCase(config);
}

/**
 * The same for an engine created naming its properties, with and without
 * unmatched results allowed.
 */
TEST_F(HashFirstReadTests, EveryCase_Named_MatchesWalkingEveryGraphFirst) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	compareEveryCase(config, &namedProperties);
	config.setAllowUnmatched(true);
	compareEveryCase(config, &namedProperties);
}

/**
 * An engine created naming its properties gives, for each kind of evidence,
 * the values an engine created naming none gives, with some properties named
 * and with every one, and with and without unmatched results allowed.
 */
TEST_F(HashFirstReadTests, EveryCase_Named_AnswersAsNoneNamed) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	std::vector<std::string> every;
	{
		RequiredPropertiesConfig properties;
		EngineHash engine(dataFilePath.c_str(), &config, &properties);
		std::unique_ptr<ResultsHash> results(engine.process(iPhoneUserAgent));
		every = results->getProperties();
	}
	ASSERT_FALSE(every.empty());
	for (const bool allowUnmatched : { false, true }) {
		config.setAllowUnmatched(allowUnmatched);
		compareNamedWithNoneNamed(config, namedProperties);
		compareNamedWithNoneNamed(config, every);
	}
}
