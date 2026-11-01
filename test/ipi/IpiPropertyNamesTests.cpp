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
 * @file IpiPropertyNamesTests.cpp
 * @brief Tests finding a property by name through the results, which is how
 * a language reads a value without keeping indexes of its own. Each reload
 * route runs in every performance profile.
 *
 * The data file reloaded has its first required property renamed. The reload
 * drops that property, as its name is no longer in the file, and the index of
 * every other property moves down by one. An index kept from before the
 * reload then reads the property after it, and the tests check that finding
 * the property through the results reads the right one.
 */

#include "../common/Base.hpp"
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/ipi.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

/** The data file committed to the data submodule, so every build has it. */
static const char namesFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** An address the data file has values for, which every lookup uses. */
static const char namesAddress[] = "1.1.1.1";

namespace {

/** The ways a caller can reload an engine. */
enum class Route { File, OriginalFile, Memory };

/** The performance profiles an engine can use. */
enum class Profile {
	InMemory,
	HighPerformance,
	LowMemory,
	Balanced,
	BalancedTemp,
	Default
};

void configure(ConfigIpi &config, const Profile profile) {
	switch (profile) {
	case Profile::InMemory: config.setMaxPerformance(); break;
	case Profile::HighPerformance: config.setHighPerformance(); break;
	case Profile::LowMemory: config.setLowMemory(); break;
	case Profile::Balanced: config.setBalanced(); break;
	case Profile::BalancedTemp: config.setBalancedTemp(); break;
	default: break;
	}
}

/** The values of properties, keyed by property name. */
typedef std::map<std::string, std::string> Values;

/** The text of a property's values with their weights, or why it has none. */
std::string describe(ResultsIpi *results, const int requiredPropertyIndex) {
	auto value = results->getValuesAsWeightedStringList(requiredPropertyIndex);
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

/** Finds a property through the results, as a language reads a value. */
int findIndex(ResultsIpi *results, const std::string &name) {
	return results->getRequiredPropertyIndex(name.c_str(), name.size());
}

/** The values of the properties named, each found through the results. */
Values readByName(ResultsIpi *results, const std::vector<std::string> &names) {
	Values values;
	for (const std::string &name : names) {
		values[name] = describe(results, findIndex(results, name));
	}
	return values;
}

/**
 * The values of the properties named, read at the index each has in the list
 * given, as a language that keeps its own map of indexes reads them.
 */
Values readAtIndexesOf(
	ResultsIpi *results,
	const std::vector<std::string> &names) {
	Values values;
	for (size_t i = 0; i < names.size(); i++) {
		values[names[i]] = describe(results, (int)i);
	}
	return values;
}

/** The number of names whose values differ. */
int countDifferences(
	const Values &expected,
	const Values &actual,
	const std::vector<std::string> &names) {
	int differences = 0;
	for (const std::string &name : names) {
		if (expected.at(name) != actual.at(name)) {
			differences++;
		}
	}
	return differences;
}

void expectSomeValues(const Values &values) {
	int found = 0;
	for (const auto &value : values) {
		if (value.second.rfind("no value", 0) != 0) {
			found++;
		}
	}
	EXPECT_GT(found, 0) <<
		"The lookup found no values, so comparing them shows nothing.";
}

}

class IpiPropertyNamesTests : public Base {
protected:
	std::vector<unsigned char> good;
	std::vector<unsigned char> renamed;
	std::vector<unsigned char> broken;
	std::string renamedName;
	std::string masterPath;
	std::string otherPath;

	void SetUp() override {
		Base::SetUp();
		const std::string path = GetFilePath(_dataFolderName, namesFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << namesFileName << "' was not found. "
				"Run 'git submodule update --init' to fetch it.";
		}
		FILE *file = fopen(path.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
		fseek(file, 0, SEEK_END);
		good.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(good.data(), 1, good.size(), file);
		fclose(file);
		ASSERT_EQ(good.size(), bytes) << "Could not read '" << path << "'.";

		// One more component in the header than the file holds, which fails
		// to load.
		broken = good;
		fiftyoneDegreesCollectionHeader components;
		const size_t position =
			offsetof(fiftyoneDegreesDataSetIpiHeader, components);
		memcpy(&components, broken.data() + position, sizeof(components));
		components.count++;
		memcpy(broken.data() + position, &components, sizeof(components));

		ASSERT_NO_FATAL_FAILURE(renameFirstProperty());

		const std::string name =
			::testing::UnitTest::GetInstance()->current_test_info()->name();
		masterPath = "ipi-names-" + name + ".ipi";
		otherPath = "ipi-names-other-" + name + ".ipi";
		ASSERT_NO_FATAL_FAILURE(writeFile(masterPath, good));
	}

	void TearDown() override {
		remove(masterPath.c_str());
		remove(otherPath.c_str());
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(good);
		std::vector<unsigned char>().swap(renamed);
		std::vector<unsigned char>().swap(broken);
		std::string().swap(renamedName);
		std::string().swap(masterPath);
		std::string().swap(otherPath);
		Base::TearDown();
	}

	static void writeFile(
		const std::string &path,
		const std::vector<unsigned char> &data) {
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(path.c_str(), data.data(), data.size()));
	}

	/**
	 * Copies the data file with the first required property renamed. The name
	 * is changed in place, so the layout of the file is the same, and the
	 * header's revision is changed so that a temporary copy of the original
	 * file is not reused in its place.
	 */
	void renameFirstProperty() {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				good.data(),
				(fiftyoneDegreesFileOffset)good.size(),
				exception));
		ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		fiftyoneDegreesDataSetIpi *dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		const fiftyoneDegreesPropertyAvailable *first =
			&dataSet->b.b.available->items[0];
		const fiftyoneDegreesString *name =
			(const fiftyoneDegreesString*)first->name.data.ptr;
		renamedName.assign(&name->value, (size_t)name->size - 1);
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesProperty *property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			first->propertyIndex,
			&item,
			exception);
		const bool found = property != nullptr && FIFTYONE_DEGREES_EXCEPTION_OKAY;
		// The first character of a stored string follows its size.
		const size_t position = found ?
			(size_t)dataSet->header.strings.startPosition +
				property->nameOffset + sizeof(uint16_t) :
			0;
		if (found) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		}
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);
		ASSERT_TRUE(found) << "The first required property was not read.";
		ASSERT_LE(position + renamedName.size(), good.size());
		ASSERT_EQ(0, memcmp(
			good.data() + position,
			renamedName.data(),
			renamedName.size())) << "The name of '" << renamedName <<
			"' is not where the header and the property say it is.";

		renamed = good;
		// Sorts after every letter, and no property name has it.
		renamed[position] = '~';
		const size_t revision =
			offsetof(fiftyoneDegreesDataSetIpiHeader, versionRevision);
		int32_t value;
		memcpy(&value, renamed.data() + revision, sizeof(value));
		value++;
		memcpy(renamed.data() + revision, &value, sizeof(value));
	}

	/** Reloads the engine from the data through the route. */
	void reload(
		EngineIpi &engine,
		const Route route,
		std::vector<unsigned char> &data) {
		switch (route) {
		case Route::File:
			writeFile(otherPath, data);
			engine.refreshData(otherPath.c_str());
			break;
		case Route::OriginalFile:
			writeFile(masterPath, data);
			engine.refreshData();
			break;
		default:
			engine.refreshData(
				(void*)data.data(),
				(fiftyoneDegreesFileOffset)data.size());
			break;
		}
	}

	/**
	 * A reload that moves the indexes. Each property found through the new
	 * results reads the value it read before the reload, where the index kept
	 * from before the reload reads another property.
	 */
	void expectReloadFindsMovedProperties(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		const std::vector<std::string> before = engine.getRequiredProperties();
		ASSERT_GT(before.size(), (size_t)2);
		ASSERT_EQ(renamedName, before[0]);
		Values expected;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			expected = readByName(results.get(), before);
		}
		expectSomeValues(expected);

		reload(engine, route, renamed);

		const std::vector<std::string> after = engine.getRequiredProperties();
		ASSERT_EQ(before.size() - 1, after.size()) <<
			"The reload must drop the renamed property.";
		std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
		EXPECT_EQ(-1, findIndex(results.get(), renamedName));
		for (size_t i = 0; i < after.size(); i++) {
			EXPECT_EQ((int)i, findIndex(results.get(), after[i])) << after[i];
		}
		EXPECT_EQ(
			countDifferences(expected, readByName(results.get(), after), after),
			0);
		EXPECT_GT(
			countDifferences(
				expected,
				readAtIndexesOf(results.get(), before),
				after),
			0) << "No index kept from before the reload reads another "
			"property, so the reload did not move the indexes.";
	}

	/**
	 * A reload that fails keeps the data set in use, and so its indexes. Each
	 * property is found where it was and reads the value it read before.
	 */
	void expectFailedReloadKeepsIndexes(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		const std::vector<std::string> names = engine.getRequiredProperties();
		Values expected;
		std::vector<int> indexes;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			expected = readByName(results.get(), names);
			for (const std::string &name : names) {
				indexes.push_back(findIndex(results.get(), name));
			}
		}
		expectSomeValues(expected);

		try {
			reload(engine, route, broken);
			ADD_FAILURE() << "No exception was thrown.";
		}
		catch (StatusCodeException &e) {
			EXPECT_NE(FIFTYONE_DEGREES_STATUS_SUCCESS, e.getCode());
		}

		EXPECT_EQ(names, engine.getRequiredProperties());
		std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
		for (size_t i = 0; i < names.size(); i++) {
			EXPECT_EQ(indexes[i], findIndex(results.get(), names[i])) << names[i];
		}
		EXPECT_EQ(expected, readByName(results.get(), names));
	}

	/**
	 * Results created before a reload keep the data set they came from, so a
	 * property found through them reads the value it read before the reload.
	 * The indexes of the data set the reload loaded read other properties.
	 */
	void expectResultsFromBeforeReloadFindProperties(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		const std::vector<std::string> before = engine.getRequiredProperties();
		std::unique_ptr<ResultsIpi> held(engine.process(namesAddress));
		const Values expected = readByName(held.get(), before);
		expectSomeValues(expected);

		reload(engine, route, renamed);

		const std::vector<std::string> after = engine.getRequiredProperties();
		ASSERT_EQ(before.size() - 1, after.size()) <<
			"The reload must drop the renamed property.";
		EXPECT_EQ(expected, readByName(held.get(), before));
		EXPECT_GT(
			countDifferences(
				expected,
				readAtIndexesOf(held.get(), after),
				after),
			0) << "No index of the reloaded data set reads another property "
			"of the results from before the reload, so the reload did not move "
			"the indexes.";
	}

	/**
	 * A graph filter given names evaluates the graph of the property named in
	 * the data set the results use, before and after a reload that moves its
	 * index.
	 */
	void expectFilterFindsNamedProperty(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		const std::vector<std::string> before = engine.getRequiredProperties();
		ASSERT_GT(before.size(), (size_t)2);
		// The reload keeps this property and moves its index from 1 to 0.
		const std::vector<std::string> named = { before[1] };
		std::string expected;
		{
			std::unique_ptr<ResultsIpi> all(engine.process(namesAddress));
			expected = describe(all.get(), findIndex(all.get(), named[0]));
			std::unique_ptr<ResultsIpi> filtered(
				engine.process(namesAddress, &named));
			EXPECT_EQ(
				expected,
				describe(filtered.get(), findIndex(filtered.get(), named[0])));
		}
		EXPECT_EQ(std::string::npos, expected.rfind("no value", 0)) <<
			"The property has no value, so filtering for it shows nothing.";

		reload(engine, route, renamed);

		std::unique_ptr<ResultsIpi> filtered(
			engine.process(namesAddress, &named));
		EXPECT_EQ(0, findIndex(filtered.get(), named[0]));
		EXPECT_EQ(
			expected,
			describe(filtered.get(), findIndex(filtered.get(), named[0])));
	}
};

/**
 * Each scenario through each route in a performance profile.
 */
#define IPI_NAMES_ROUTE_TESTS(r, p) \
TEST_F(IpiPropertyNamesTests, r##_##p##_Reload_FindsMovedProperties) { \
	expectReloadFindsMovedProperties(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, r##_##p##_FailedReload_KeepsIndexes) { \
	expectFailedReloadKeepsIndexes(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, r##_##p##_ResultsFromBeforeReload_FindProperties) { \
	expectResultsFromBeforeReloadFindProperties(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, r##_##p##_Filter_FindsNamedProperty) { \
	expectFilterFindsNamedProperty(Route::r, Profile::p); \
}

#define IPI_NAMES_PROFILE_TESTS(p) \
IPI_NAMES_ROUTE_TESTS(File, p) \
IPI_NAMES_ROUTE_TESTS(OriginalFile, p) \
IPI_NAMES_ROUTE_TESTS(Memory, p)

IPI_NAMES_PROFILE_TESTS(InMemory)
IPI_NAMES_PROFILE_TESTS(HighPerformance)
IPI_NAMES_PROFILE_TESTS(LowMemory)
IPI_NAMES_PROFILE_TESTS(Balanced)
IPI_NAMES_PROFILE_TESTS(BalancedTemp)
IPI_NAMES_PROFILE_TESTS(Default)

/**
 * Every required property is found at its own index, whatever the case of the
 * name, and only by its whole name.
 */
TEST_F(IpiPropertyNamesTests, Find_EveryName) {
	ConfigIpi config;
	RequiredPropertiesConfig properties;
	EngineIpi engine(masterPath.c_str(), &config, &properties);
	const std::vector<std::string> names = engine.getRequiredProperties();
	std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
	for (size_t i = 0; i < names.size(); i++) {
		std::string upper = names[i];
		std::string lower = names[i];
		for (char &c : upper) {
			c = (char)toupper((unsigned char)c);
		}
		for (char &c : lower) {
			c = (char)tolower((unsigned char)c);
		}
		EXPECT_EQ((int)i, findIndex(results.get(), names[i])) << names[i];
		EXPECT_EQ((int)i, findIndex(results.get(), upper)) << upper;
		EXPECT_EQ((int)i, findIndex(results.get(), lower)) << lower;
		EXPECT_EQ(-1, findIndex(results.get(), names[i] + "X")) << names[i];
		EXPECT_EQ(-1, findIndex(results.get(), names[i].substr(
			0,
			names[i].size() - 1))) << names[i];
	}
}

/**
 * The name is the bytes given, so it needs no null at its end, and nothing
 * is found for no name.
 */
TEST_F(IpiPropertyNamesTests, Find_TakesTheBytesGiven) {
	ConfigIpi config;
	RequiredPropertiesConfig properties;
	EngineIpi engine(masterPath.c_str(), &config, &properties);
	const std::vector<std::string> names = engine.getRequiredProperties();
	ASSERT_FALSE(names.empty());
	std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
	const std::string followed = names[0] + "|more";
	EXPECT_EQ(0, results->getRequiredPropertyIndex(
		followed.data(),
		names[0].size()));
	EXPECT_EQ(-1, results->getRequiredPropertyIndex(followed.data(), 0));
	EXPECT_EQ(-1, results->getRequiredPropertyIndex(nullptr, 3));
	EXPECT_EQ(-1, fiftyoneDegreesResultsIpiGetRequiredPropertyIndex(
		nullptr,
		names[0].c_str(),
		names[0].size()));
}

/**
 * No names evaluates every graph, an empty list evaluates none, and a name
 * that is not a required property evaluates none.
 */
TEST_F(IpiPropertyNamesTests, Filter_NamesSelectGraphs) {
	ConfigIpi config;
	RequiredPropertiesConfig properties;
	EngineIpi engine(masterPath.c_str(), &config, &properties);
	const std::vector<std::string> names = engine.getRequiredProperties();
	std::unique_ptr<ResultsIpi> all(engine.process(namesAddress));
	const Values expected = readByName(all.get(), names);
	expectSomeValues(expected);
	std::unique_ptr<ResultsIpi> unnamed(engine.process(
		namesAddress,
		(const std::vector<std::string>*)nullptr));
	EXPECT_EQ(expected, readByName(unnamed.get(), names));

	const std::vector<std::string> none;
	const std::vector<std::string> unknown = { "NotAProperty" };
	std::unique_ptr<ResultsIpi> empty(engine.process(namesAddress, &none));
	std::unique_ptr<ResultsIpi> missing(engine.process(namesAddress, &unknown));
	const int indexes[] = { 0 };
	std::unique_ptr<ResultsIpi> noIndexes(
		engine.process(namesAddress, indexes, 0));
	EXPECT_EQ(
		readByName(noIndexes.get(), names),
		readByName(empty.get(), names));
	EXPECT_EQ(
		readByName(noIndexes.get(), names),
		readByName(missing.get(), names));
}
