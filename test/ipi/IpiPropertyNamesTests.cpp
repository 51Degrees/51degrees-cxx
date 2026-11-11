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
 * @brief Tests reading properties by name and by position across reloads,
 * through each reload route in every performance profile.
 *
 * The data file reloaded has its first required property renamed, so the
 * reload drops that property and moves every other property in the data
 * set's sorted list. A position is the place of a property in the names the
 * caller gave, so it reads the same property before and after the reload,
 * and the dropped property reads as one the data set does not have until a
 * later file has it again. An engine that names no properties reads by name,
 * and a position reads nothing.
 */

#include "../common/Base.hpp"
#include <algorithm>
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

/** The free ASN data file, which the data repository's script downloads. */
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

/** The text of a list of weighted values, or why there is none. */
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

/** What a read gives for a property the data set does not have. */
const std::string invalidProperty = "no value, reason " +
	std::to_string(
		(int)FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY);

/** The value of the property at the position given. */
std::string describeAt(ResultsIpi *results, const int requiredPropertyIndex) {
	return describe(
		results->getValuesAsWeightedStringList(requiredPropertyIndex));
}

/** The value of the property named, read by name through the results. */
std::string describeByName(ResultsIpi *results, const std::string &name) {
	return describe(results->getValuesAsWeightedStringList(name));
}

/** The values of the properties named, each read by name. */
Values readByName(ResultsIpi *results, const std::vector<std::string> &names) {
	Values values;
	for (const std::string &name : names) {
		values[name] = describeByName(results, name);
	}
	return values;
}

/**
 * The values of the properties named, each read at its place in the list
 * given, as a language that keeps the order of the names it gave reads them.
 */
Values readAtPositions(
	ResultsIpi *results,
	const std::vector<std::string> &names) {
	Values values;
	for (size_t i = 0; i < names.size(); i++) {
		values[names[i]] = describeAt(results, (int)i);
	}
	return values;
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

/** The count and the names alongside the positions follow the names given. */
void expectNamesAtPositions(
	ResultsIpi *results,
	const std::vector<std::string> &names) {
	EXPECT_EQ((int)names.size(), results->getAvailableProperties());
	for (size_t i = 0; i < names.size(); i++) {
		EXPECT_EQ(names[i], results->getPropertyName((int)i)) << i;
	}
}

}

class IpiPropertyNamesTests : public Base {
protected:
	std::vector<unsigned char> good;
	std::vector<unsigned char> renamed;
	std::vector<unsigned char> broken;
	std::string renamedName;
	std::string newName;
	std::vector<std::string> sortedNames;
	std::vector<std::string> callerNames;
	std::string masterPath;
	std::string otherPath;

	void SetUp() override {
		Base::SetUp();
		const std::string path = GetFilePath(_dataFolderName, namesFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << namesFileName << "' was not found. "
				"Run data/get-data-files.sh or .ps1 to download it.";
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
		std::string().swap(newName);
		std::vector<std::string>().swap(sortedNames);
		std::vector<std::string>().swap(callerNames);
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
	 * file is not reused in its place. Also sets the names the tests give: the
	 * properties of the file in sorted order, and the same names in an order
	 * the sort does not give, with the renamed property before another name.
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
		const fiftyoneDegreesPropertiesAvailable *available =
			dataSet->b.b.available;
		for (uint32_t i = 0; i < available->count; i++) {
			const fiftyoneDegreesString *name = (const fiftyoneDegreesString*)
				available->items[i].name.data.ptr;
			sortedNames.push_back(
				std::string(&name->value, (size_t)name->size - 1));
		}
		const fiftyoneDegreesPropertyAvailable *first = &available->items[0];
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
		ASSERT_GE(sortedNames.size(), (size_t)2);
		renamedName = sortedNames[0];
		ASSERT_LE(position + renamedName.size(), good.size());
		ASSERT_EQ(0, memcmp(
			good.data() + position,
			renamedName.data(),
			renamedName.size())) << "The name of '" << renamedName <<
			"' is not where the header and the property say it is.";

		renamed = good;
		// Sorts after every letter, and no property name has it.
		renamed[position] = '~';
		newName = renamedName;
		newName[0] = '~';
		const size_t revision =
			offsetof(fiftyoneDegreesDataSetIpiHeader, versionRevision);
		int32_t value;
		memcpy(&value, renamed.data() + revision, sizeof(value));
		value++;
		memcpy(renamed.data() + revision, &value, sizeof(value));

		// Reversed, then the renamed property, now last, moved before the
		// name that follows it when there is one.
		callerNames.assign(sortedNames.rbegin(), sortedNames.rend());
		if (callerNames.size() >= 3) {
			std::rotate(
				callerNames.begin() + 1,
				callerNames.end() - 1,
				callerNames.end());
		}
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
	 * A reload that drops a property and moves the others in the data set's
	 * sorted list. Each position reads the property it read before, the
	 * dropped property reads as one the data set does not have while keeping
	 * its place, and it reads its value again once a later file has it.
	 */
	void expectPositionsSurviveReload(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties(&callerNames);
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		EXPECT_EQ(callerNames, engine.getRequiredProperties());
		Values expected;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			EXPECT_EQ(sortedNames, results->getProperties());
			expectNamesAtPositions(results.get(), callerNames);
			expected = readByName(results.get(), callerNames);
			EXPECT_EQ(expected, readAtPositions(results.get(), callerNames));
		}
		expectSomeValues(expected);

		reload(engine, route, renamed);

		EXPECT_EQ(callerNames, engine.getRequiredProperties()) <<
			"A name the data file does not have must keep its place.";
		Values missing = expected;
		missing[renamedName] = invalidProperty;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			EXPECT_FALSE(results->containsProperty(renamedName));
			expectNamesAtPositions(results.get(), callerNames);
			EXPECT_EQ(missing, readAtPositions(results.get(), callerNames));
			EXPECT_EQ(missing, readByName(results.get(), callerNames));
		}

		reload(engine, route, good);

		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			EXPECT_TRUE(results->containsProperty(renamedName)) <<
				"A later data file with the property must make it available.";
			EXPECT_EQ(expected, readAtPositions(results.get(), callerNames));
		}
	}

	/**
	 * An engine that names no properties takes the first data file's
	 * properties as its names, in the data set's order. A reload that drops
	 * a property and moves the others keeps each position on its property,
	 * the dropped one reading as one the data set does not have, and the
	 * property that only the new file holds gets no position, so its name
	 * reads as invalid too.
	 */
	void expectNothingNamedKeepsPositions(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		EXPECT_EQ(sortedNames, engine.getRequiredProperties());
		Values expected;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			EXPECT_EQ(sortedNames, results->getProperties());
			expectNamesAtPositions(results.get(), sortedNames);
			expected = readByName(results.get(), sortedNames);
			EXPECT_EQ(expected, readAtPositions(results.get(), sortedNames));
		}
		expectSomeValues(expected);

		reload(engine, route, renamed);

		EXPECT_EQ(sortedNames, engine.getRequiredProperties()) <<
			"The reload must keep the first file's names, in their order.";
		Values missing = expected;
		missing[renamedName] = invalidProperty;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			EXPECT_FALSE(results->containsProperty(renamedName));
			EXPECT_FALSE(results->containsProperty(newName));
			expectNamesAtPositions(results.get(), sortedNames);
			EXPECT_EQ(missing, readAtPositions(results.get(), sortedNames));
			EXPECT_EQ(missing, readByName(results.get(), sortedNames));
			EXPECT_EQ(invalidProperty, describeByName(results.get(), newName));
		}
	}

	/**
	 * A reload that fails keeps the data set in use, so each position reads
	 * the value it read before.
	 */
	void expectFailedReloadKeepsPositions(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties(&callerNames);
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		Values expected;
		{
			std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
			expected = readByName(results.get(), callerNames);
			EXPECT_EQ(expected, readAtPositions(results.get(), callerNames));
		}
		expectSomeValues(expected);

		try {
			reload(engine, route, broken);
			ADD_FAILURE() << "No exception was thrown.";
		}
		catch (StatusCodeException &e) {
			EXPECT_NE(FIFTYONE_DEGREES_STATUS_SUCCESS, e.getCode());
		}

		EXPECT_EQ(callerNames, engine.getRequiredProperties());
		std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
		EXPECT_EQ(sortedNames, results->getProperties());
		expectNamesAtPositions(results.get(), callerNames);
		EXPECT_EQ(expected, readByName(results.get(), callerNames));
		EXPECT_EQ(expected, readAtPositions(results.get(), callerNames));
	}

	/**
	 * Results created before a reload keep the data set they came from, and
	 * a position reads the same property through them as through the results
	 * of the data set the reload loaded.
	 */
	void expectResultsFromBeforeReloadKeepPositions(
		const Route route,
		const Profile profile) {
		ConfigIpi config;
		configure(config, profile);
		RequiredPropertiesConfig properties(&callerNames);
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		std::unique_ptr<ResultsIpi> held(engine.process(namesAddress));
		const Values expected = readByName(held.get(), callerNames);
		EXPECT_EQ(expected, readAtPositions(held.get(), callerNames));
		expectSomeValues(expected);

		reload(engine, route, renamed);

		EXPECT_EQ(sortedNames, held->getProperties());
		EXPECT_EQ(expected, readByName(held.get(), callerNames));
		EXPECT_EQ(expected, readAtPositions(held.get(), callerNames));
		Values missing = expected;
		missing[renamedName] = invalidProperty;
		std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
		EXPECT_EQ(missing, readAtPositions(results.get(), callerNames));
	}
};

/**
 * Each scenario through each route in a performance profile.
 */
#define IPI_NAMES_ROUTE_TESTS(r, p) \
TEST_F(IpiPropertyNamesTests, r##_##p##_Reload_KeepsPositions) { \
	expectPositionsSurviveReload(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, r##_##p##_NothingNamed_KeepsPositions) { \
	expectNothingNamedKeepsPositions(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, r##_##p##_FailedReload_KeepsPositions) { \
	expectFailedReloadKeepsPositions(Route::r, Profile::p); \
} \
TEST_F(IpiPropertyNamesTests, \
	r##_##p##_ResultsFromBeforeReload_KeepPositions) { \
	expectResultsFromBeforeReloadKeepPositions(Route::r, Profile::p); \
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
 * Each position reads the property at that place in the names given, whatever
 * the case of the name, and a position outside the names reads nothing.
 */
TEST_F(IpiPropertyNamesTests, Positions_EveryNameAtItsPlace) {
	ConfigIpi config;
	RequiredPropertiesConfig properties(&callerNames);
	EngineIpi engine(masterPath.c_str(), &config, &properties);
	std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
	EXPECT_EQ(sortedNames, results->getProperties()) <<
		"The data set's own list of properties stays sorted.";
	for (size_t i = 0; i < callerNames.size(); i++) {
		std::string upper = callerNames[i];
		std::string lower = callerNames[i];
		for (char &c : upper) {
			c = (char)toupper((unsigned char)c);
		}
		for (char &c : lower) {
			c = (char)tolower((unsigned char)c);
		}
		const std::string expected =
			describeByName(results.get(), callerNames[i]);
		EXPECT_EQ(expected, describeAt(results.get(), (int)i)) <<
			callerNames[i];
		EXPECT_EQ(expected, describeByName(results.get(), upper)) << upper;
		EXPECT_EQ(expected, describeByName(results.get(), lower)) << lower;
	}
	EXPECT_EQ(invalidProperty, describeAt(results.get(), -1));
	EXPECT_EQ(
		invalidProperty,
		describeAt(results.get(), (int)callerNames.size()));
	EXPECT_EQ(
		std::string(),
		results->getPropertyName((int)callerNames.size()));
}

/**
 * Every required property is found by name, whatever the case of the name,
 * and only by its whole name.
 */
TEST_F(IpiPropertyNamesTests, Find_EveryName) {
	ConfigIpi config;
	RequiredPropertiesConfig properties;
	EngineIpi engine(masterPath.c_str(), &config, &properties);
	const std::vector<std::string> names = engine.getRequiredProperties();
	std::unique_ptr<ResultsIpi> results(engine.process(namesAddress));
	EXPECT_EQ(names, results->getProperties());
	for (size_t i = 0; i < names.size(); i++) {
		std::string upper = names[i];
		std::string lower = names[i];
		for (char &c : upper) {
			c = (char)toupper((unsigned char)c);
		}
		for (char &c : lower) {
			c = (char)tolower((unsigned char)c);
		}
		const std::string expected = describeByName(results.get(), names[i]);
		EXPECT_TRUE(results->containsProperty(names[i])) << names[i];
		EXPECT_TRUE(results->containsProperty(upper)) << upper;
		EXPECT_TRUE(results->containsProperty(lower)) << lower;
		EXPECT_EQ(expected, describeByName(results.get(), upper)) << upper;
		EXPECT_EQ(expected, describeByName(results.get(), lower)) << lower;
		EXPECT_FALSE(results->containsProperty(names[i] + "X")) << names[i];
		EXPECT_FALSE(results->containsProperty(names[i].substr(
			0,
			names[i].size() - 1))) << names[i];
	}
}
