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
 * @file HashMatchMetricsTests.cpp
 * @brief Tests of the match metrics and matched User-Agents of Hash results,
 * read through the C functions and through ResultsHash. The answers are
 * compared with the result items the walks set, and with metrics written into
 * the items, which differ so that a sum, a highest value and a single item's
 * value each give a different answer. Every reader walks the graphs not yet
 * walked, so each is also read first from results that have walked none.
 */

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/hash.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

const char iPhoneUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53";

/**
 * The iPhone User-Agent with two characters added after the product, so that
 * its substrings are found away from where they are expected.
 */
const char shiftedUserAgent[] =
	"Mozilla/5.0 xx(iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53";

const char chromeUserAgent[] =
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36";

/** The drift the engines allow, so that a match can be found with drift. */
const int32_t allowedDriftCharacters = 10;

/** The difference the engines allow, so that a match can have one. */
const int32_t allowedDifference = 10;

/** Properties of each component, for the engine that names them. */
const std::vector<std::string> namedProperties = {
	"IsMobile",
	"HardwareName",
	"PlatformName",
	"BrowserName",
	"IsCrawler" };

/** Each metric of one result, or of several taken together. */
struct Metrics {
	fiftyoneDegreesHashMatchMethod method;
	int32_t difference;
	int32_t drift;
	int32_t iterations;
	int32_t matchedNodes;
};

bool operator==(const Metrics& a, const Metrics& b) {
	return a.method == b.method &&
		a.difference == b.difference &&
		a.drift == b.drift &&
		a.iterations == b.iterations &&
		a.matchedNodes == b.matchedNodes;
}

std::ostream& operator<<(std::ostream& stream, const Metrics& metrics) {
	return stream << "{ method " << (int)metrics.method <<
		", difference " << metrics.difference <<
		", drift " << metrics.drift <<
		", iterations " << metrics.iterations <<
		", matched nodes " << metrics.matchedNodes << " }";
}

/** The metrics the readers give when there is no result to read. */
const Metrics noMetrics = {
	FIFTYONE_DEGREES_HASH_MATCH_METHOD_NONE, 0, 0, 0, 0 };

/** Metrics no reader gives, to show that a reader sets each one. */
const Metrics unsetMetrics = {
	FIFTYONE_DEGREES_HASH_MATCH_METHODS_LENGTH, -1, -1, -1, -1 };

/** The metrics of the result item. */
Metrics metricsOfItem(const fiftyoneDegreesResultHash& item) {
	return {
		item.method,
		item.difference,
		item.drift,
		item.iterations,
		item.matchedNodes };
}

/** The highest method and the sum of each other metric of the items. */
Metrics metricsOfItems(const fiftyoneDegreesResultsHash* results) {
	Metrics total = noMetrics;
	for (uint32_t i = 0; i < results->count; i++) {
		const Metrics item = metricsOfItem(results->items[i]);
		if (item.method > total.method) {
			total.method = item.method;
		}
		total.difference += item.difference;
		total.drift += item.drift;
		total.iterations += item.iterations;
		total.matchedNodes += item.matchedNodes;
	}
	return total;
}

/** The metrics the C function gives for every result. */
Metrics metricsOf(fiftyoneDegreesResultsHash* results) {
	Metrics metrics = unsetMetrics;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashGetMatchMetrics(
		results,
		&metrics.method,
		&metrics.difference,
		&metrics.drift,
		&metrics.iterations,
		&metrics.matchedNodes,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return metrics;
}

/** The metrics the C function gives for one result, and if it has them. */
Metrics metricsOfResult(
	fiftyoneDegreesResultsHash* results,
	uint32_t resultIndex,
	bool* found) {
	Metrics metrics = unsetMetrics;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	*found = fiftyoneDegreesResultsHashGetMatchMetricsForResult(
		results,
		resultIndex,
		&metrics.method,
		&metrics.difference,
		&metrics.drift,
		&metrics.iterations,
		&metrics.matchedNodes,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return metrics;
}

/** The matched User-Agent the C function gives for one result. */
const char* matchedUserAgentOf(
	fiftyoneDegreesResultsHash* results,
	uint32_t resultIndex) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const char* userAgent = fiftyoneDegreesResultsHashGetMatchedUserAgent(
		results,
		resultIndex,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return userAgent;
}

/** The components whose graph is still to be walked, in order. */
std::vector<int> pendingComponents(const fiftyoneDegreesResultsHash* results) {
	std::vector<int> components;
	for (uint32_t word = 0;
		word < FIFTYONE_DEGREES_RESULTS_PENDING_WORDS;
		word++) {
		for (uint32_t bit = 0; bit < 32; bit++) {
			if ((results->b.b.pending[word] & ((uint32_t)1 << bit)) != 0) {
				components.push_back((int)(word * 32 + bit));
			}
		}
	}
	return components;
}

/** The components walked, in order, by the results that record walks. */
std::vector<int>* walkLog = nullptr;

/** The walk the results made before they recorded their walks. */
fiftyoneDegreesResultsEvaluateMethod walkMethod = nullptr;

/** Records the component in the log, then walks it. */
void recordWalk(
	fiftyoneDegreesResultsBase* results,
	unsigned char componentIndex,
	fiftyoneDegreesException* exception) {
	walkLog->push_back((int)componentIndex);
	walkMethod(results, componentIndex, exception);
}

/** Makes the results record each walk in the log given. */
void recordWalks(fiftyoneDegreesResultsHash* results, std::vector<int>& log) {
	walkMethod = results->b.b.evaluate;
	walkLog = &log;
	results->b.b.evaluate = recordWalk;
}

/** A walk that fails, as one that cannot read the data file does. */
void failWalk(
	fiftyoneDegreesResultsBase*,
	unsigned char,
	fiftyoneDegreesException* exception) {
	FIFTYONE_DEGREES_EXCEPTION_SET(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA);
}

/**
 * The methods written into the items, so that the highest is neither the
 * first nor the last of four.
 */
const fiftyoneDegreesHashMatchMethod writtenMethods[] = {
	FIFTYONE_DEGREES_HASH_MATCH_METHOD_COMBINED,
	FIFTYONE_DEGREES_HASH_MATCH_METHOD_PREDICTIVE,
	FIFTYONE_DEGREES_HASH_MATCH_METHOD_PERFORMANCE,
	FIFTYONE_DEGREES_HASH_MATCH_METHOD_NONE };

const uint32_t writtenMethodsCount =
	(uint32_t)(sizeof(writtenMethods) / sizeof(writtenMethods[0]));

/**
 * Writes metrics into each item, one bit for each item in a different place
 * for each metric, so that every sum differs from every single item's value
 * and from every other metric's sum.
 */
void writeMetrics(fiftyoneDegreesResultsHash* results) {
	for (uint32_t i = 0; i < results->count; i++) {
		fiftyoneDegreesResultHash& item = results->items[i];
		item.method = writtenMethods[i % writtenMethodsCount];
		item.difference = (int32_t)1 << i;
		item.drift = (int32_t)1 << (i + 8);
		item.iterations = (int32_t)1 << (i + 16);
		item.matchedNodes = (int32_t)i + 1;
	}
}

/**
 * The metrics of every result after writeMetrics, for results with at least
 * two items.
 */
Metrics writtenTotal(uint32_t count) {
	const int32_t bits = (int32_t)(((uint32_t)1 << count) - 1);
	return {
		FIFTYONE_DEGREES_HASH_MATCH_METHOD_PREDICTIVE,
		bits,
		bits << 8,
		bits << 16,
		(int32_t)(count * (count + 1) / 2) };
}

/** The answers of one reader of the results. */
typedef std::vector<std::string> Answers;

/** A reader of the results, by the name of what it reads. */
typedef std::pair<
	std::string,
	std::function<Answers(ResultsHash*)>> Reader;

Answers answer(int value) {
	return { std::to_string(value) };
}

/** The getter's answer for each result, then for the index past the last. */
Answers eachResult(
	ResultsHash* results,
	const std::function<std::string(uint32_t)>& get) {
	Answers answers;
	const uint32_t count = (uint32_t)results->getUserAgents();
	for (uint32_t i = 0; i <= count; i++) {
		answers.push_back(get(i));
	}
	return answers;
}

/**
 * Every reader of the metrics and matched User-Agents of ResultsHash. The
 * matched User-Agent is also read for the index -1.
 */
std::vector<Reader> readers() {
	return {
		{ "getIterations", [](ResultsHash* r) {
			return answer(r->getIterations()); } },
		{ "getMatchedNodes", [](ResultsHash* r) {
			return answer(r->getMatchedNodes()); } },
		{ "getDrift", [](ResultsHash* r) {
			return answer(r->getDrift()); } },
		{ "getDifference", [](ResultsHash* r) {
			return answer(r->getDifference()); } },
		{ "getMethod", [](ResultsHash* r) {
			return answer(r->getMethod()); } },
		{ "getUserAgents", [](ResultsHash* r) {
			return answer(r->getUserAgents()); } },
		{ "getDrift(index)", [](ResultsHash* r) {
			return eachResult(r, [r](uint32_t i) {
				return std::to_string(r->getDrift(i)); }); } },
		{ "getDifference(index)", [](ResultsHash* r) {
			return eachResult(r, [r](uint32_t i) {
				return std::to_string(r->getDifference(i)); }); } },
		{ "getMethod(index)", [](ResultsHash* r) {
			return eachResult(r, [r](uint32_t i) {
				return std::to_string(r->getMethod(i)); }); } },
		{ "getUserAgent(index)", [](ResultsHash* r) {
			Answers answers = { r->getUserAgent(-1) };
			for (const std::string& userAgent : eachResult(r, [r](uint32_t i) {
				return r->getUserAgent((int)i); })) {
				answers.push_back(userAgent);
			}
			return answers; } } };
}

/**
 * What each reader should give, in the order of readers, from the result
 * items. Past the last result each metric is zero and the matched User-Agent
 * is empty, as it is for the index -1.
 */
std::vector<Answers> answersOfItems(const fiftyoneDegreesResultsHash* results) {
	const Metrics total = metricsOfItems(results);
	Answers drifts, differences, methods, userAgents = { "" };
	for (uint32_t i = 0; i < results->count; i++) {
		const fiftyoneDegreesResultHash& item = results->items[i];
		drifts.push_back(std::to_string(item.drift));
		differences.push_back(std::to_string(item.difference));
		methods.push_back(std::to_string((int)item.method));
		userAgents.push_back(item.b.matchedUserAgent == nullptr ?
			std::string() : std::string(item.b.matchedUserAgent));
	}
	drifts.push_back("0");
	differences.push_back("0");
	methods.push_back("0");
	userAgents.push_back("");
	return {
		answer(total.iterations),
		answer(total.matchedNodes),
		answer(total.drift),
		answer(total.difference),
		answer((int)total.method),
		answer((int)results->count),
		drifts,
		differences,
		methods,
		userAgents };
}

/** The evidence of one case, as the keys and values the engine is given. */
typedef std::vector<std::pair<std::string, std::string>> EvidenceList;

}

/**
 * Tests through the C functions on the first Hash data file found, with
 * every property available, so that populating the results walks no graph.
 * A skip in SetUp does not stop the test with the Visual Studio googletest,
 * so each test first checks that SetUp created the manager.
 */
class HashMatchMetricsCTests : public Base {
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
		config.b.updateMatchedUserAgent = true;
		config.b.allowUnmatched = false;
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

	/** Populates the results from the User-Agent, walking no graph. */
	static void populate(
		fiftyoneDegreesResultsHash* results,
		const char* userAgent) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			userAgent,
			strlen(userAgent),
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}

	/** New results populated from the User-Agent. */
	fiftyoneDegreesResultsHash* detect(const char* userAgent) {
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&manager, 0, 0);
		populate(results, userAgent);
		return results;
	}

	static void walkEveryGraph(fiftyoneDegreesResultsHash* results) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&results->b.b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}
};

/**
 * The method is the highest any result used, and each other metric is the
 * sum of every result's. One result's metrics are its own, and each metric
 * asked for alone is the one asked for.
 */
TEST_F(HashMatchMetricsCTests, Metrics_SumEveryResultAndTakeTheHighestMethod) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* results = detect(iPhoneUserAgent);
	walkEveryGraph(results);
	ASSERT_GE(results->count, 2u) <<
		"A User-Agent should give a result for each of several components.";
	writeMetrics(results);
	const Metrics total = writtenTotal(results->count);
	EXPECT_EQ(total, metricsOf(results));

	Metrics alone = unsetMetrics;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashGetMatchMetrics(results,
		&alone.method, nullptr, nullptr, nullptr, nullptr, exception);
	fiftyoneDegreesResultsHashGetMatchMetrics(results,
		nullptr, &alone.difference, nullptr, nullptr, nullptr, exception);
	fiftyoneDegreesResultsHashGetMatchMetrics(results,
		nullptr, nullptr, &alone.drift, nullptr, nullptr, exception);
	fiftyoneDegreesResultsHashGetMatchMetrics(results,
		nullptr, nullptr, nullptr, &alone.iterations, nullptr, exception);
	fiftyoneDegreesResultsHashGetMatchMetrics(results,
		nullptr, nullptr, nullptr, nullptr, &alone.matchedNodes, exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	EXPECT_EQ(total, alone);

	for (uint32_t i = 0; i < results->count; i++) {
		bool found = false;
		EXPECT_EQ(
			metricsOfItem(results->items[i]),
			metricsOfResult(results, i, &found)) << "Result " << i;
		EXPECT_TRUE(found) << "Result " << i;
	}
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * Each reader walks every graph not yet walked, in component order, and
 * gives what the items hold once every graph is walked.
 */
TEST_F(HashMatchMetricsCTests, Readers_WalkEveryGraphNotYetWalked) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* walked = detect(iPhoneUserAgent);
	walkEveryGraph(walked);
	ASSERT_GT(walked->count, 0u);
	fiftyoneDegreesResultsHash* results = detect(iPhoneUserAgent);
	std::vector<int> walks;
	recordWalks(results, walks);
	const std::vector<int> pending = pendingComponents(results);
	ASSERT_FALSE(pending.empty()) << "Populating the results walked a graph.";

	EXPECT_EQ(metricsOfItems(walked), metricsOf(results));
	EXPECT_EQ(pending, walks) << "Reading the metrics of every result.";
	EXPECT_TRUE(pendingComponents(results).empty());

	populate(results, iPhoneUserAgent);
	walks.clear();
	bool found = false;
	EXPECT_EQ(
		metricsOfItem(walked->items[0]),
		metricsOfResult(results, 0, &found));
	EXPECT_TRUE(found);
	EXPECT_EQ(pending, walks) << "Reading the metrics of one result.";

	populate(results, iPhoneUserAgent);
	walks.clear();
	EXPECT_STREQ(
		walked->items[0].b.matchedUserAgent,
		matchedUserAgentOf(results, 0));
	EXPECT_EQ(pending, walks) << "Reading the matched User-Agent.";

	populate(results, iPhoneUserAgent);
	walks.clear();
	EXPECT_EQ(nullptr, matchedUserAgentOf(results, results->count));
	EXPECT_EQ(pending, walks) << "Reading past the last result.";

	fiftyoneDegreesResultsHashFree(results);
	fiftyoneDegreesResultsHashFree(walked);
}

/**
 * When a walk fails, the exception says so, every metric is zero, the method
 * is none and there is no matched User-Agent.
 */
TEST_F(HashMatchMetricsCTests, Readers_GiveNothingWhenAWalkFails) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* results = detect(iPhoneUserAgent);
	ASSERT_FALSE(pendingComponents(results).empty());
	results->b.b.evaluate = failWalk;
	{
		Metrics metrics = unsetMetrics;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashGetMatchMetrics(
			results,
			&metrics.method,
			&metrics.difference,
			&metrics.drift,
			&metrics.iterations,
			&metrics.matchedNodes,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED);
		EXPECT_EQ(noMetrics, metrics);
	}
	populate(results, iPhoneUserAgent);
	{
		Metrics metrics = unsetMetrics;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_FALSE(fiftyoneDegreesResultsHashGetMatchMetricsForResult(
			results,
			0,
			&metrics.method,
			&metrics.difference,
			&metrics.drift,
			&metrics.iterations,
			&metrics.matchedNodes,
			exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED);
		EXPECT_EQ(noMetrics, metrics);
	}
	populate(results, iPhoneUserAgent);
	{
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(nullptr, fiftyoneDegreesResultsHashGetMatchedUserAgent(
			results,
			0,
			exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED);
	}
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * With no results, no result at the index, or no evidence, every metric is
 * zero, the method is none and there is no matched User-Agent, with no
 * exception.
 */
TEST_F(HashMatchMetricsCTests, Readers_GiveNothingWithoutAResult) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	bool found = true;
	EXPECT_EQ(noMetrics, metricsOf(nullptr));
	EXPECT_EQ(noMetrics, metricsOfResult(nullptr, 0, &found));
	EXPECT_FALSE(found);
	EXPECT_EQ(nullptr, matchedUserAgentOf(nullptr, 0));

	fiftyoneDegreesResultsHash* results = detect(iPhoneUserAgent);
	found = true;
	EXPECT_EQ(noMetrics, metricsOfResult(results, results->count, &found));
	EXPECT_FALSE(found);
	EXPECT_EQ(nullptr, matchedUserAgentOf(results, results->count));

	fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
		fiftyoneDegreesEvidenceCreate(1);
	{
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromEvidence(results, evidence, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}
	EXPECT_EQ(0u, results->count);
	EXPECT_EQ(noMetrics, metricsOf(results));
	EXPECT_EQ(noMetrics, metricsOfResult(results, 0, &found));
	EXPECT_FALSE(found);
	EXPECT_EQ(nullptr, matchedUserAgentOf(results, 0));
	fiftyoneDegreesEvidenceFree(evidence);
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * The matched User-Agent of a result is the one the result holds, made of the
 * characters of the User-Agent with those the match did not use replaced by
 * '_', and there is none when the result records none.
 */
TEST_F(HashMatchMetricsCTests, MatchedUserAgent_IsTheResultsOwn) {
	if (managerReady == false) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResultsHash* results = detect(iPhoneUserAgent);
	const size_t userAgentLength = strlen(iPhoneUserAgent);
	walkEveryGraph(results);
	ASSERT_GT(results->count, 0u);
	for (uint32_t i = 0; i < results->count; i++) {
		const char* matched = matchedUserAgentOf(results, i);
		ASSERT_NE(nullptr, matched) << "Result " << i;
		EXPECT_EQ(
			(const char*)results->items[i].b.matchedUserAgent,
			matched) << "Result " << i;
		const size_t matchedLength = strlen(matched);
		const size_t length = matchedLength < userAgentLength ?
			matchedLength : userAgentLength;
		for (size_t c = 0; c < length; c++) {
			EXPECT_TRUE(
				matched[c] == '_' || matched[c] == iPhoneUserAgent[c]) <<
				"Result " << i << ", character " << c << " of '" << matched <<
				"'";
		}
	}
	char* recorded = results->items[0].b.matchedUserAgent;
	results->items[0].b.matchedUserAgent = nullptr;
	EXPECT_EQ(nullptr, matchedUserAgentOf(results, 0));
	results->items[0].b.matchedUserAgent = recorded;
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * Tests through the C++ classes on the first Hash data file found. A skip in
 * SetUp does not stop the test with the Visual Studio googletest, so each
 * test first checks that SetUp found a data file.
 */
class HashMatchMetricsTests : public Base {
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

	/** The C results the ResultsHash reads. */
	static fiftyoneDegreesResultsHash* itemsOf(ResultsHash* results) {
		return results->results;
	}

	static void walkEveryGraph(ResultsHash* results) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&results->results->b.b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}

	/**
	 * Each kind of evidence, by name, using the device id the engine gives
	 * the iPhone.
	 */
	static std::vector<std::pair<std::string, EvidenceList>> everyCase(
		const EngineHash& engine) {
		std::string deviceId;
		{
			std::unique_ptr<ResultsHash> first(engine.process(iPhoneUserAgent));
			deviceId = first->getDeviceId();
		}
		return {
			{ "iPhone", { { "header.User-Agent", iPhoneUserAgent } } },
			{ "shifted iPhone", { { "header.User-Agent", shiftedUserAgent } } },
			{ "Chrome", { { "header.User-Agent", chromeUserAgent } } },
			{ "client hints", {
				{ "header.User-Agent", chromeUserAgent },
				{ "header.Sec-CH-UA",
					"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" },
				{ "header.Sec-CH-UA-Mobile", "?0" },
				{ "header.Sec-CH-UA-Platform", "\"Windows\"" },
				{ "header.Sec-CH-UA-Platform-Version", "\"14.0.0\"" } } },
			{ "device id", { { "query.51D_deviceId", deviceId } } },
			{ "unknown User-Agent", { { "header.User-Agent", "unknown" } } },
			{ "no evidence", { } } };
	}
};

/**
 * Each getter gives what the result items hold once every graph is walked,
 * taking the highest method and the sum of each other metric, whether the
 * graphs were walked before it was read or it is the first thing read, for
 * each kind of evidence, with drift and difference allowed, and with the
 * properties named and with none named.
 */
TEST_F(HashMatchMetricsTests, Getters_GiveWhatTheItemsHold) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::vector<Reader> every = readers();
	for (const bool named : { false, true }) {
		ConfigHash config;
		config.setBalanced();
		config.setUsePerformanceGraph(true);
		config.setDrift(allowedDriftCharacters);
		config.setDifference(allowedDifference);
		config.setUpdateMatchedUserAgent(true);
		std::unique_ptr<RequiredPropertiesConfig> properties(named ?
			new RequiredPropertiesConfig(&namedProperties) :
			new RequiredPropertiesConfig());
		EngineHash engine(dataFilePath.c_str(), &config, properties.get());
		for (const auto& c : everyCase(engine)) {
			EvidenceDeviceDetection evidence;
			for (const auto& pair : c.second) {
				evidence[pair.first] = pair.second;
			}
			std::unique_ptr<ResultsHash> walked(engine.process(&evidence));
			walkEveryGraph(walked.get());
			const std::vector<Answers> expected =
				answersOfItems(itemsOf(walked.get()));
			ASSERT_EQ(every.size(), expected.size());
			for (size_t i = 0; i < every.size(); i++) {
				EXPECT_EQ(expected[i], every[i].second(walked.get())) <<
					c.first << ", " << every[i].first <<
					(named ? ", named" : "");
				std::unique_ptr<ResultsHash> unread(engine.process(&evidence));
				EXPECT_EQ(expected[i], every[i].second(unread.get())) <<
					c.first << ", " << every[i].first << " read first" <<
					(named ? ", named" : "");
			}
		}
	}
}

/**
 * With metrics written into the items, the getters give the highest method
 * and the sum of each other metric, and each result's own metrics by index.
 */
TEST_F(HashMatchMetricsTests, Getters_SumEveryResultAndTakeTheHighestMethod) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	RequiredPropertiesConfig properties;
	EngineHash engine(dataFilePath.c_str(), &config, &properties);
	std::unique_ptr<ResultsHash> results(engine.process(iPhoneUserAgent));
	walkEveryGraph(results.get());
	fiftyoneDegreesResultsHash* items = itemsOf(results.get());
	ASSERT_GE(items->count, 2u) <<
		"A User-Agent should give a result for each of several components.";
	writeMetrics(items);
	const Metrics total = writtenTotal(items->count);
	EXPECT_EQ(total.iterations, results->getIterations());
	EXPECT_EQ(total.matchedNodes, results->getMatchedNodes());
	EXPECT_EQ(total.drift, results->getDrift());
	EXPECT_EQ(total.difference, results->getDifference());
	EXPECT_EQ((int)total.method, results->getMethod());
	for (uint32_t i = 0; i < items->count; i++) {
		EXPECT_EQ(items->items[i].drift, results->getDrift(i)) << i;
		EXPECT_EQ(items->items[i].difference, results->getDifference(i)) << i;
		EXPECT_EQ((int)items->items[i].method, results->getMethod(i)) << i;
	}
}
