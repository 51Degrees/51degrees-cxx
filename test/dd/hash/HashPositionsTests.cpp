/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is the subject of the following patents and patent
 * applications, owned by 51 Degrees Mobile Experts Limited of 5 Charlotte
 * Close, Caversham, Reading, Berkshire, United Kingdom RG4 7BY:
 * European Patent No. 3438848; and
 * United States Patent No. 10,482,175.
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
 * @file HashPositionsTests.cpp
 * @brief Tests that a required property index is the place of the property
 * in the names the caller gave, through the C reads and across reloads
 * between the TAC and Lite data files, which have different properties.
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/hash.h"
#include "../../../src/dd/hash/ResultsHashSerializer.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

/** The data file with the fewest properties. */
static const char smallFileName[] = "51Degrees-LiteV4.1.hash";

/** The data file with the most properties. */
static const char largeFileName[] = "TAC-HashV41.hash";

/** A name that no data file has. */
static const char missingName[] = "NoSuchProperty";

static const char *positionUserAgents[] = {
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53",
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36"
};

namespace {

typedef std::map<std::string, std::string> Values;
typedef std::map<std::string, Values> Lookups;

/** Results that are freed on every way out of a test. */
typedef std::unique_ptr<
	fiftyoneDegreesResultsHash,
	decltype(&fiftyoneDegreesResultsHashFree)> OwnedResults;

/**
 * Frees an initialised manager on every way out of a test. Declare it before
 * any results made from the manager, so that the results are freed first.
 */
typedef std::unique_ptr<
	fiftyoneDegreesResourceManager,
	decltype(&fiftyoneDegreesResourceManagerFree)> ManagerGuard;

const std::string invalidProperty = "no value, reason " +
	std::to_string(
		(int)FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY);

std::string describe(const Value<std::vector<std::string>> &value) {
	if (value.hasValue() == false) {
		return "no value, reason " +
			std::to_string((int)value.getNoValueReason());
	}
	std::string text;
	for (const std::string &item : value.getValue()) {
		text.append(item).append("|");
	}
	return text;
}

Values readByName(ResultsHash *results, const std::vector<std::string> &names) {
	Values values;
	for (const std::string &name : names) {
		values[name] = describe(results->getValues(name));
	}
	return values;
}

Values readAtPositions(
	ResultsHash *results,
	const std::vector<std::string> &names) {
	Values values;
	for (size_t i = 0; i < names.size(); i++) {
		values[names[i]] = describe(results->getValues((int)i));
	}
	return values;
}

std::vector<std::string> fileNames(const std::string &path) {
	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig properties;
	EngineHash engine(path.c_str(), &config, &properties);
	return engine.getRequiredProperties();
}

Lookups freshByName(
	const std::string &path,
	const std::vector<std::string> &names) {
	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig properties(&names);
	EngineHash engine(path.c_str(), &config, &properties);
	Lookups lookups;
	for (const char *userAgent : positionUserAgents) {
		std::unique_ptr<ResultsHash> results(engine.process(userAgent));
		lookups[userAgent] = readByName(results.get(), names);
	}
	return lookups;
}

Lookups atPositions(EngineHash &engine, const std::vector<std::string> &names) {
	Lookups lookups;
	for (const char *userAgent : positionUserAgents) {
		std::unique_ptr<ResultsHash> results(engine.process(userAgent));
		lookups[userAgent] = readAtPositions(results.get(), names);
	}
	return lookups;
}

}

class HashPositionsTests : public Base {
protected:
	std::string smallPath;
	std::vector<std::string> smallNames;

	void SetUp() override {
		Base::SetUp();
		smallPath = GetFilePath(_dataFolderName, smallFileName);
		if (smallPath.empty()) {
			GTEST_SKIP() << "The file '" << smallFileName << "' was not found.";
		}
		smallNames = fileNames(smallPath);
		ASSERT_GE(smallNames.size(), (size_t)2);
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::string().swap(smallPath);
		std::vector<std::string>().swap(smallNames);
		Base::TearDown();
	}

	/** Creates a manager from the small file with the names given. */
	void initSmall(
		fiftyoneDegreesResourceManager *manager,
		const char *names) {
		initFile(manager, smallPath, names);
	}

	/** Creates a manager from the file with the names given. */
	static void initFile(
		fiftyoneDegreesResourceManager *manager,
		const std::string &path,
		const char *names) {
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = names;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				manager,
				&config,
				&properties,
				path.c_str(),
				exception));
	}

	static std::string valuesByName(
		fiftyoneDegreesResultsHash *results,
		const char *name) {
		std::vector<char> buffer(1 << 18, '\0');
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashGetValuesString(
			results,
			name,
			buffer.data(),
			buffer.size(),
			(char*)"|",
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << name;
		return std::string(buffer.data());
	}

	static std::string valuesAt(
		fiftyoneDegreesResultsHash *results,
		int requiredPropertyIndex) {
		std::vector<char> buffer(1 << 18, '\0');
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashGetValuesStringByRequiredPropertyIndex(
			results,
			requiredPropertyIndex,
			buffer.data(),
			buffer.size(),
			(char*)"|",
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << requiredPropertyIndex;
		return std::string(buffer.data());
	}

	/** The names reversed, after a name no data file has. */
	std::vector<std::string> callerNames() const {
		std::vector<std::string> names(smallNames.rbegin(), smallNames.rend());
		names.insert(names.begin(), missingName);
		return names;
	}

	static std::string join(const std::vector<std::string> &names) {
		std::string joined;
		for (const std::string &name : names) {
			joined.append(joined.empty() ? "" : ",").append(name);
		}
		return joined;
	}
};

/**
 * Through the C reads, the index of a name given is its place among the
 * names given, a name the data does not have keeps its place, and the name
 * and values at each index are those of the property named there.
 */
TEST_F(HashPositionsTests, C_IndexesFollowTheNamesGiven) {
	const std::vector<std::string> names = callerNames();
	const std::string joined = join(names);
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, joined.c_str()));
	ManagerGuard managerGuard(&manager, fiftyoneDegreesResourceManagerFree);
	OwnedResults results(
		fiftyoneDegreesResultsHashCreate(&manager, 0, 0),
		fiftyoneDegreesResultsHashFree);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromUserAgent(
		results.get(),
		positionUserAgents[0],
		strlen(positionUserAgents[0]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesPropertiesAvailable *available =
		((fiftyoneDegreesDataSetHash*)results->b.b.dataSet)->b.b.available;

	EXPECT_EQ(-1, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		available,
		missingName));
	for (size_t i = 1; i < names.size(); i++) {
		ASSERT_EQ(
			(int)i,
			fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
				available,
				names[i].c_str())) << names[i];
		const fiftyoneDegreesString *name =
			fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
				available,
				(int)i);
		ASSERT_NE(nullptr, name) << i;
		EXPECT_EQ(names[i], std::string(&name->value)) << i;
		EXPECT_EQ(
			valuesByName(results.get(), names[i].c_str()),
			valuesAt(results.get(), (int)i)) << names[i];
	}

	const fiftyoneDegreesString *missing =
		fiftyoneDegreesPropertiesGetNameFromRequiredIndex(available, 0);
	ASSERT_NE(nullptr, missing);
	EXPECT_STREQ(missingName, &missing->value);
	EXPECT_FALSE(fiftyoneDegreesResultsHashGetHasValues(
		results.get(),
		0,
		exception));
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
		fiftyoneDegreesResultsHashGetNoValueReason(
			results.get(),
			0,
			exception));
}

/**
 * The string of every property has one line for each index of the names
 * given, in order, and an empty line for a name the data does not have.
 */
TEST_F(HashPositionsTests, C_AllPropertiesFollowTheNamesGiven) {
	const std::vector<std::string> names = callerNames();
	const std::string joined = join(names);
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, joined.c_str()));
	fiftyoneDegreesResultsHash *results =
		fiftyoneDegreesResultsHashCreate(&manager, 0, 0);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromUserAgent(
		results,
		positionUserAgents[0],
		strlen(positionUserAgents[0]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);

	std::vector<char> buffer(1 << 22, '\0');
	fiftyoneDegreesResultsHashGetValuesStringAllProperties(
		results,
		buffer.data(),
		buffer.size(),
		(char*)"|",
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);

	// An empty line for the name the data does not have, then the values of
	// each name given, in order. The whole text is compared, rather than
	// split at each line end, so a value holding that character cannot shift
	// the lines.
	std::string expected = "\r";
	for (size_t i = 1; i < names.size(); i++) {
		expected.append(valuesByName(results, names[i].c_str())).append("\r");
	}
	const std::string actual(buffer.data());
	size_t same = 0;
	while (same < expected.size() && same < actual.size() &&
		expected[same] == actual[same]) {
		same++;
	}
	EXPECT_EQ(expected.size(), actual.size());
	EXPECT_EQ(expected.size(), same) << "The text differs from character " <<
		same << ", expected '" << expected.substr(same, 80) << "', found '" <<
		actual.substr(same, 80) << "'";

	fiftyoneDegreesResultsHashFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}

/**
 * An engine that named no properties has an index for each property of the
 * data file, in the data set's order, so an index reads the property its
 * name reads.
 */
TEST_F(HashPositionsTests, Cxx_NothingNamedReadsByIndex) {
	ConfigHash config;
	RequiredPropertiesConfig properties;
	EngineHash engine(smallPath.c_str(), &config, &properties);
	std::unique_ptr<ResultsHash> results(engine.process(positionUserAgents[0]));
	EXPECT_EQ(smallNames, results->getProperties());
	int found = 0;
	for (size_t i = 0; i < smallNames.size(); i++) {
		const Value<std::vector<std::string>> byName =
			results->getValues(smallNames[i]);
		found += byName.hasValue() ? 1 : 0;
		EXPECT_EQ(describe(byName), describe(results->getValues((int)i))) << i;
		EXPECT_EQ(
			results->getValueAsString(smallNames[i]).hasValue(),
			results->getValueAsString((int)i).hasValue()) << i;
		EXPECT_EQ(smallNames[i], results->getPropertyName((int)i)) << i;
	}
	EXPECT_GT(found, 0) << "No property had a value by name.";
}

/** Starting value of the fingerprint, from the 64 bit FNV-1a hash. */
static const uint64_t jsonFingerprintOffsetBasis = 14695981039346656037ULL;

/** Multiplier of the fingerprint, from the 64 bit FNV-1a hash. */
static const uint64_t jsonFingerprintPrime = 1099511628211ULL;

/** A fingerprint of the text, so that runs can be compared from their logs. */
static uint64_t jsonFingerprint(const std::string &text) {
	uint64_t hash = jsonFingerprintOffsetBasis;
	for (const char c : text) {
		hash ^= (unsigned char)c;
		hash *= jsonFingerprintPrime;
	}
	return hash;
}

/**
 * The JSON of every property is the same whatever the order of the names
 * given, and with a name the data does not have, as it is written from the
 * data set's own list of its properties. The fingerprint of each JSON is
 * written out, so that the JSON of different builds can be compared.
 */
TEST_F(HashPositionsTests, Json_DoesNotDependOnTheOrderOfTheNames) {
	const std::vector<std::string> given = callerNames();
	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig sortedProperties(&smallNames);
	RequiredPropertiesConfig givenProperties(&given);
	EngineHash sortedEngine(smallPath.c_str(), &config, &sortedProperties);
	EngineHash givenEngine(smallPath.c_str(), &config, &givenProperties);
	ResultsHashSerializer serializer;
	for (const char *userAgent : positionUserAgents) {
		std::unique_ptr<ResultsHash> sorted(sortedEngine.process(userAgent));
		std::unique_ptr<ResultsHash> inGivenOrder(
			givenEngine.process(userAgent));
		const std::string expected = serializer.allValuesJson(sorted.get());
		EXPECT_FALSE(expected.empty()) << userAgent;
		EXPECT_EQ(expected, serializer.allValuesJson(inGivenOrder.get())) <<
			userAgent;
		std::printf(
			"JSON of %s, %u characters, fingerprint %016llx\n",
			userAgent,
			(unsigned int)expected.size(),
			(unsigned long long)jsonFingerprint(expected));
		std::fflush(stdout);
	}
}

/**
 * The reload tests need the TAC data file as well.
 */
class HashPositionsReloadTests : public HashPositionsTests {
protected:
	std::string largePath;
	std::vector<std::string> names;
	std::string largeOnly;

	void SetUp() override {
		HashPositionsTests::SetUp();
		// The set up above skips only when the small file is missing, which is
		// checked here as the Visual Studio googletest has no IsSkipped.
		if (smallPath.empty() || HasFatalFailure()) {
			return;
		}
		largePath = GetFilePath(_dataFolderName, largeFileName);
		if (largePath.empty()) {
			GTEST_SKIP() << "The file '" << largeFileName << "' was not found.";
		}
		const std::vector<std::string> largeNames = fileNames(largePath);
		std::vector<std::string> shared;
		for (const std::string &name : largeNames) {
			if (std::find(smallNames.begin(), smallNames.end(), name) ==
				smallNames.end()) {
				if (largeOnly.empty()) {
					largeOnly = name;
				}
			}
			else {
				shared.push_back(name);
			}
		}
		if (largeOnly.empty() || shared.size() < 2) {
			GTEST_SKIP() << "The data files do not differ in the way the "
				"tests need.";
		}
		names = { largeOnly, shared.back(), shared.front() };
	}

	void TearDown() override {
		std::string().swap(largePath);
		std::vector<std::string>().swap(names);
		std::string().swap(largeOnly);
		HashPositionsTests::TearDown();
	}
};

/**
 * An engine on the TAC file reloaded with the Lite file, then the TAC file
 * again, reads each property at the index the caller gave it every time, as
 * a fresh engine on the same file reads it by name. The property the Lite
 * file lacks reads as invalid there, and comes back with the TAC file.
 */
TEST_F(HashPositionsReloadTests, Named_LargeThenSmallThenLarge) {
	// A skip in SetUp does not stop the test with the Visual Studio
	// googletest, and the names are only set once SetUp has finished.
	if (names.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const Lookups onLarge = freshByName(largePath, names);
	const Lookups onSmall = freshByName(smallPath, names);
	for (const auto &lookup : onSmall) {
		EXPECT_EQ(invalidProperty, lookup.second.at(largeOnly));
	}

	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig properties(&names);
	EngineHash engine(largePath.c_str(), &config, &properties);
	EXPECT_EQ(names, engine.getRequiredProperties());
	EXPECT_EQ(onLarge, atPositions(engine, names));

	engine.refreshData(smallPath.c_str());
	EXPECT_EQ(names, engine.getRequiredProperties());
	EXPECT_EQ(onSmall, atPositions(engine, names));

	engine.refreshData(largePath.c_str());
	EXPECT_EQ(onLarge, atPositions(engine, names));
	std::unique_ptr<ResultsHash> results(engine.process(positionUserAgents[0]));
	EXPECT_TRUE(results->containsProperty(largeOnly)) <<
		"The property did not come back with the file that has it.";
}

/**
 * An engine that named nothing, reloaded from the Lite file to the TAC file,
 * keeps the Lite file's properties as its names, in the same order, so each
 * index reads what a fresh engine on the TAC file reads by that name. A
 * property that only the TAC file holds gets no index, so it reads as
 * invalid.
 */
TEST_F(HashPositionsReloadTests, NothingNamed_SmallThenLarge) {
	// A skip in SetUp does not stop the test with the Visual Studio
	// googletest, and the names are only set once SetUp has finished.
	if (names.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	Lookups expected;
	{
		ConfigHash config;
		config.setLowMemory();
		RequiredPropertiesConfig properties;
		EngineHash fresh(largePath.c_str(), &config, &properties);
		for (const char *userAgent : positionUserAgents) {
			std::unique_ptr<ResultsHash> results(fresh.process(userAgent));
			expected[userAgent] = readByName(results.get(), smallNames);
		}
	}

	ConfigHash config;
	config.setLowMemory();
	RequiredPropertiesConfig properties;
	EngineHash engine(smallPath.c_str(), &config, &properties);
	EXPECT_EQ(smallNames, engine.getRequiredProperties());
	engine.refreshData(largePath.c_str());
	EXPECT_EQ(smallNames, engine.getRequiredProperties()) <<
		"The reload must keep the first file's names, in their order.";
	EXPECT_EQ(expected, atPositions(engine, smallNames));
	for (const char *userAgent : positionUserAgents) {
		std::unique_ptr<ResultsHash> results(engine.process(userAgent));
		EXPECT_EQ(expected[userAgent], readByName(results.get(), smallNames));
		EXPECT_EQ(invalidProperty, describe(results->getValues(largeOnly)));
	}
}

/**
 * Through the C reads, each index of an engine that named nothing names the
 * same property after a reload from the Lite file to the TAC file, whose
 * properties sort into other places, and reads what a fresh engine on the
 * TAC file reads by that name.
 */
TEST_F(HashPositionsReloadTests, C_NothingNamed_SmallThenLarge) {
	// A skip in SetUp does not stop the test with the Visual Studio
	// googletest, and the names are only set once SetUp has finished.
	if (names.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	fiftyoneDegreesResourceManager fresh;
	ASSERT_NO_FATAL_FAILURE(initFile(&fresh, largePath, nullptr));
	ManagerGuard freshGuard(&fresh, fiftyoneDegreesResourceManagerFree);
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, nullptr));
	ManagerGuard managerGuard(&manager, fiftyoneDegreesResourceManagerFree);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesHashReloadManagerFromFile(
			&manager,
			largePath.c_str(),
			exception));

	for (const char *userAgent : positionUserAgents) {
		OwnedResults expected(
			fiftyoneDegreesResultsHashCreate(&fresh, 0, 0),
			fiftyoneDegreesResultsHashFree);
		OwnedResults results(
			fiftyoneDegreesResultsHashCreate(&manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		fiftyoneDegreesResultsHashFromUserAgent(
			expected.get(),
			userAgent,
			strlen(userAgent),
			exception);
		fiftyoneDegreesResultsHashFromUserAgent(
			results.get(),
			userAgent,
			strlen(userAgent),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		fiftyoneDegreesPropertiesAvailable *available =
			((fiftyoneDegreesDataSetHash*)results->b.b.dataSet)->b.b.available;
		ASSERT_EQ(
			(uint32_t)smallNames.size(),
			fiftyoneDegreesPropertiesGetRequiredPropertyCount(available));
		for (size_t i = 0; i < smallNames.size(); i++) {
			const fiftyoneDegreesString *name =
				fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
					available,
					(int)i);
			ASSERT_NE(nullptr, name) << i;
			EXPECT_EQ(smallNames[i], std::string(&name->value)) << i;
			EXPECT_EQ(
				valuesByName(expected.get(), smallNames[i].c_str()),
				valuesAt(results.get(), (int)i)) << smallNames[i];
		}
	}
}
