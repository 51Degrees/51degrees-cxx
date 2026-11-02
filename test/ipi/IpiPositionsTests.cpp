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
 * @file IpiPositionsTests.cpp
 * @brief Tests that a required property index is the place of the property
 * in the names the caller gave, through the C reads and across reloads
 * between data files with different properties.
 *
 * The reload tests move between the Enterprise data file and the small data
 * file committed to the data submodule, which has fewer properties, as the
 * Go reload cases moved between the Enterprise and Lite files.
 */

#include "../common/Base.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/ipi_weighted_results.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

/** The small data file committed to the data submodule. */
static const char smallFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** The Enterprise data file, which the CI fetches. */
static const char largeFileName[] = "51Degrees-EnterpriseIpiV41.ipi";

/** A name that no data file has. */
static const char missingName[] = "NoSuchProperty";

/** The addresses the Go reload cases looked up. */
static const char *positionAddresses[] = {
	"185.28.167.77",
	"8.8.8.8",
	"81.2.69.142",
	"2001:4860:4860::8888"
};

namespace {

/** The values of properties, keyed by property name. */
typedef std::map<std::string, std::string> Values;

/** The values for each address, keyed by address. */
typedef std::map<std::string, Values> Lookups;

const std::string invalidProperty = "no value, reason " +
	std::to_string(
		(int)FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY);

template <typename T>
std::string describe(const T &value) {
	if (value.hasValue() == false) {
		return "no value, reason " +
			std::to_string((int)value.getNoValueReason());
	}
	std::string text;
	for (const auto &weighted : value.getValue()) {
		text.append(weighted.getValue())
			.append("@")
			.append(std::to_string(weighted.getRawWeight()))
			.append("|");
	}
	return text;
}

Values readByName(ResultsIpi *results, const std::vector<std::string> &names) {
	Values values;
	for (const std::string &name : names) {
		values[name] = describe(results->getValuesAsWeightedStringList(name));
	}
	return values;
}

Values readAtPositions(
	ResultsIpi *results,
	const std::vector<std::string> &names) {
	Values values;
	for (size_t i = 0; i < names.size(); i++) {
		values[names[i]] = describe(
			results->getValuesAsWeightedStringList((int)i));
	}
	return values;
}

/** The names a fresh engine on the file has, in sorted order. */
std::vector<std::string> fileNames(const std::string &path) {
	ConfigIpi config;
	config.setLowMemory();
	RequiredPropertiesConfig properties;
	EngineIpi engine(path.c_str(), &config, &properties);
	return engine.getRequiredProperties();
}

/** What a fresh engine on the file reads by name for each address. */
Lookups freshByName(
	const std::string &path,
	const std::vector<std::string> &names) {
	ConfigIpi config;
	config.setLowMemory();
	RequiredPropertiesConfig properties(&names);
	EngineIpi engine(path.c_str(), &config, &properties);
	Lookups lookups;
	for (const char *address : positionAddresses) {
		std::unique_ptr<ResultsIpi> results(engine.process(address));
		lookups[address] = readByName(results.get(), names);
	}
	return lookups;
}

/** What the engine reads at each position for each address. */
Lookups atPositions(EngineIpi &engine, const std::vector<std::string> &names) {
	Lookups lookups;
	for (const char *address : positionAddresses) {
		std::unique_ptr<ResultsIpi> results(engine.process(address));
		lookups[address] = readAtPositions(results.get(), names);
	}
	return lookups;
}

}

/**
 * The C tests use the small data file, which every build has.
 */
class IpiPositionsTests : public Base {
protected:
	std::string smallPath;
	std::vector<std::string> smallNames;

	void SetUp() override {
		Base::SetUp();
		smallPath = GetFilePath(_dataFolderName, smallFileName);
		if (smallPath.empty()) {
			GTEST_SKIP() << "The file '" << smallFileName << "' was not "
				"found. Run 'git submodule update --init' to fetch it.";
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
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = names;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
				manager,
				&config,
				&properties,
				smallPath.c_str(),
				exception));
	}

	static std::string valuesByName(
		fiftyoneDegreesResultsIpi *results,
		const char *name) {
		char buffer[4096] = "";
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiGetValuesString(
			results,
			name,
			buffer,
			sizeof(buffer),
			"|",
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << name;
		return std::string(buffer);
	}

	static std::string valuesAt(
		fiftyoneDegreesResultsIpi *results,
		int requiredPropertyIndex) {
		char buffer[4096] = "";
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
			results,
			requiredPropertyIndex,
			buffer,
			sizeof(buffer),
			"|",
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << requiredPropertyIndex;
		return std::string(buffer);
	}
};

/**
 * Through the C reads, the index of a name given is its place among the
 * names given, a name the data does not have keeps its place, and the name
 * and values at each index are those of the property named there.
 */
TEST_F(IpiPositionsTests, C_IndexesFollowTheNamesGiven) {
	// Reversed, after a name no file has.
	std::vector<std::string> names(smallNames.rbegin(), smallNames.rend());
	names.insert(names.begin(), missingName);
	std::string joined;
	for (const std::string &name : names) {
		joined.append(joined.empty() ? "" : ",").append(name);
	}
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, joined.c_str()));
	fiftyoneDegreesResultsIpi *results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		positionAddresses[1],
		strlen(positionAddresses[1]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesPropertiesAvailable *available =
		((fiftyoneDegreesDataSetIpi*)results->b.dataSet)->b.b.available;

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
			valuesByName(results, names[i].c_str()),
			valuesAt(results, (int)i)) << names[i];
	}

	// The name the data does not have reads as a property the data set does
	// not have, and the names after it keep their places.
	const fiftyoneDegreesString *missing =
		fiftyoneDegreesPropertiesGetNameFromRequiredIndex(available, 0);
	ASSERT_NE(nullptr, missing);
	EXPECT_STREQ(missingName, &missing->value);
	EXPECT_EQ(-1, fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
		available,
		0));
	EXPECT_FALSE(fiftyoneDegreesResultsIpiGetHasValues(results, 0, exception));
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
		fiftyoneDegreesResultsIpiGetNoValueReason(results, 0, exception));
	EXPECT_EQ(std::string(), valuesAt(results, 0));
	EXPECT_EQ(nullptr, fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
		available,
		(int)names.size()));

	fiftyoneDegreesResultsIpiFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}

/**
 * The weighted values collection takes the indexes of the names given, its
 * values carry the index they were read for, and an index with no property
 * gives no values rather than failing the whole collection.
 */
TEST_F(IpiPositionsTests, C_CollectionTakesTheIndexesOfTheNamesGiven) {
	std::vector<std::string> names(smallNames.rbegin(), smallNames.rend());
	names.insert(names.begin(), missingName);
	std::string joined;
	for (const std::string &name : names) {
		joined.append(joined.empty() ? "" : ",").append(name);
	}
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, joined.c_str()));
	fiftyoneDegreesResultsIpi *results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		positionAddresses[1],
		strlen(positionAddresses[1]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);

	// Count the values each index has, and the index each value is for.
	std::map<int, int> expected;
	for (size_t i = 1; i < names.size(); i++) {
		const fiftyoneDegreesWeightedItem *items =
			fiftyoneDegreesResultsIpiGetValues(results, (int)i, exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << names[i];
		expected[(int)i] = items == nullptr ? 0 : (int)results->values.count;
	}

	std::vector<int> indexes;
	for (size_t i = 0; i < names.size(); i++) {
		indexes.push_back((int)(names.size() - 1 - i));
	}
	fiftyoneDegreesWeightedValuesCollection collection =
		fiftyoneDegreesResultsIpiGetValuesCollection(
			results,
			indexes.data(),
			(uint32_t)indexes.size(),
			nullptr,
			exception);
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"An index with no property failed the whole collection.";
	std::map<int, int> actual;
	for (uint32_t i = 0; i < collection.itemsCount; i++) {
		actual[collection.items[i]->requiredPropertyIndex]++;
	}
	for (const auto &count : expected) {
		EXPECT_EQ(count.second, actual[count.first]) <<
			names[count.first];
	}
	EXPECT_EQ(0, actual[0]) << "The name the data does not have has values.";
	fiftyoneDegreesWeightedValuesCollectionRelease(&collection);

	// With no indexes the collection reads every index of the names given.
	fiftyoneDegreesWeightedValuesCollection all =
		fiftyoneDegreesResultsIpiGetValuesCollection(
			results,
			nullptr,
			0,
			nullptr,
			exception);
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	std::map<int, int> allCounts;
	for (uint32_t i = 0; i < all.itemsCount; i++) {
		allCounts[all.items[i]->requiredPropertyIndex]++;
	}
	for (const auto &count : expected) {
		EXPECT_EQ(count.second, allCounts[count.first]) << names[count.first];
	}
	fiftyoneDegreesWeightedValuesCollectionRelease(&all);

	fiftyoneDegreesResultsIpiFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}

/**
 * With no names given, the index of a property is its place in the sorted
 * list of the data set's properties, as it always was.
 */
TEST_F(IpiPositionsTests, C_NothingNamedIndexesAreTheSortedPlaces) {
	fiftyoneDegreesResourceManager manager;
	ASSERT_NO_FATAL_FAILURE(initSmall(&manager, nullptr));
	fiftyoneDegreesResultsIpi *results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		positionAddresses[1],
		strlen(positionAddresses[1]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesPropertiesAvailable *available =
		((fiftyoneDegreesDataSetIpi*)results->b.dataSet)->b.b.available;
	ASSERT_EQ(smallNames.size(), (size_t)available->count);
	for (size_t i = 0; i < smallNames.size(); i++) {
		EXPECT_EQ(
			(int)i,
			fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
				available,
				smallNames[i].c_str())) << smallNames[i];
		EXPECT_EQ(
			valuesByName(results, smallNames[i].c_str()),
			valuesAt(results, (int)i)) << smallNames[i];
	}
	fiftyoneDegreesResultsIpiFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}

/**
 * An engine that named no properties has no required property indexes, so
 * an index reads nothing, while a name reads its values.
 */
TEST_F(IpiPositionsTests, Cxx_NothingNamedRefusesIndexes) {
	ConfigIpi config;
	RequiredPropertiesConfig properties;
	EngineIpi engine(smallPath.c_str(), &config, &properties);
	std::unique_ptr<ResultsIpi> results(engine.process(positionAddresses[1]));
	EXPECT_EQ(smallNames, results->getProperties());
	EXPECT_EQ((int)smallNames.size(), results->getAvailableProperties());
	int found = 0;
	for (size_t i = 0; i < smallNames.size(); i++) {
		const auto byName =
			results->getValuesAsWeightedStringList(smallNames[i]);
		found += byName.hasValue() ? 1 : 0;
		EXPECT_EQ(
			invalidProperty,
			describe(results->getValuesAsWeightedStringList((int)i))) << i;
		EXPECT_FALSE(results->getValueAsString((int)i).hasValue()) << i;
		EXPECT_FALSE(results->getValues((int)i).hasValue()) << i;
		EXPECT_FALSE(results->getValueAsUTF8String((int)i).hasValue()) << i;
		EXPECT_EQ(std::string(), results->getPropertyName((int)i)) << i;
	}
	EXPECT_GT(found, 0) << "No property had a value by name.";
}

/**
 * The Go reload cases need the Enterprise data file as well.
 */
class IpiPositionsReloadTests : public IpiPositionsTests {
protected:
	std::string largePath;
	std::vector<std::string> names;
	std::string largeOnly;

	void SetUp() override {
		IpiPositionsTests::SetUp();
		if (IsSkipped()) {
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
		// A property only the Enterprise file has first, then two that both
		// have, in an order the sort does not give.
		names = { largeOnly, shared.back(), shared.front() };
	}

	void TearDown() override {
		std::string().swap(largePath);
		std::vector<std::string>().swap(names);
		std::string().swap(largeOnly);
		IpiPositionsTests::TearDown();
	}
};

/**
 * The named case. An engine on the Enterprise file reloaded with the small
 * file, then the Enterprise file again, reads each property at the index the
 * caller gave it every time, as a fresh engine on the same file reads it by
 * name. The property the small file lacks reads as invalid there, and comes
 * back with the Enterprise file.
 */
TEST_F(IpiPositionsReloadTests, Named_LargeThenSmallThenLarge) {
	const Lookups onLarge = freshByName(largePath, names);
	const Lookups onSmall = freshByName(smallPath, names);
	for (const auto &lookup : onSmall) {
		EXPECT_EQ(invalidProperty, lookup.second.at(largeOnly));
	}

	ConfigIpi config;
	config.setLowMemory();
	RequiredPropertiesConfig properties(&names);
	EngineIpi engine(largePath.c_str(), &config, &properties);
	EXPECT_EQ(names, engine.getRequiredProperties());
	EXPECT_EQ(onLarge, atPositions(engine, names));

	engine.refreshData(smallPath.c_str());
	EXPECT_EQ(names, engine.getRequiredProperties());
	EXPECT_EQ(onSmall, atPositions(engine, names));
	{
		std::unique_ptr<ResultsIpi> results(
			engine.process(positionAddresses[1]));
		EXPECT_EQ((int)names.size(), results->getAvailableProperties());
		for (size_t i = 0; i < names.size(); i++) {
			EXPECT_EQ(names[i], results->getPropertyName((int)i)) << i;
		}
		EXPECT_FALSE(results->containsProperty(largeOnly));
	}

	engine.refreshData(largePath.c_str());
	EXPECT_EQ(onLarge, atPositions(engine, names));
	std::unique_ptr<ResultsIpi> results(engine.process(positionAddresses[1]));
	EXPECT_TRUE(results->containsProperty(largeOnly)) <<
		"The property did not come back with the file that has it.";
}

/**
 * The case with nothing named. An engine on the small file reloaded with the
 * Enterprise file reads every property by name as a fresh engine on the
 * Enterprise file does, has every property of that file, and reads nothing
 * by index before or after.
 */
TEST_F(IpiPositionsReloadTests, NothingNamed_SmallThenLarge) {
	const std::vector<std::string> largeNames = fileNames(largePath);
	Lookups expected;
	{
		ConfigIpi config;
		config.setLowMemory();
		RequiredPropertiesConfig properties;
		EngineIpi fresh(largePath.c_str(), &config, &properties);
		for (const char *address : positionAddresses) {
			std::unique_ptr<ResultsIpi> results(fresh.process(address));
			expected[address] = readByName(results.get(), largeNames);
		}
	}

	ConfigIpi config;
	config.setLowMemory();
	RequiredPropertiesConfig properties;
	EngineIpi engine(smallPath.c_str(), &config, &properties);
	{
		std::unique_ptr<ResultsIpi> results(
			engine.process(positionAddresses[1]));
		EXPECT_EQ(
			invalidProperty,
			describe(results->getValuesAsWeightedStringList(0)));
	}

	engine.refreshData(largePath.c_str());
	EXPECT_EQ(largeNames, engine.getRequiredProperties()) <<
		"The reload must make every property of the new file available.";
	for (const char *address : positionAddresses) {
		std::unique_ptr<ResultsIpi> results(engine.process(address));
		EXPECT_EQ(expected[address], readByName(results.get(), largeNames)) <<
			address;
		EXPECT_EQ(
			invalidProperty,
			describe(results->getValuesAsWeightedStringList(0))) << address;
	}
}
