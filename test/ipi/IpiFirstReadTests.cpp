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
 * @file IpiFirstReadTests.cpp
 * @brief Tests that the results walk a component's graph the first time one
 * of its properties is read, rather than when they are populated. The walks
 * are counted by wrapping the method the results call to walk a component.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/constantsIpi.h"

/** Addresses the data files hold answers for, of both versions. */
static const char* const firstReadAddresses[] = {
	"50.154.29.201",
	"1.1.1.1",
	"8.8.8.8",
	"185.28.167.77",
	"2001:4860:4860::8888",
	"::ffff:50.154.29.201",
	"0.0.0.0",
	"255.255.255.255"
};

/** The free ASN data file, which the data repository's script downloads. */
static const char firstReadSmallFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

namespace {

/** The components walked, in order, by the results that count their walks. */
std::vector<int>* walkLog = nullptr;

/** The walk the results make when they are not counting. */
fiftyoneDegreesResultsEvaluateMethod walkMethod = nullptr;

/** Records the component in the log, then walks it. */
void countWalk(
	fiftyoneDegreesResultsBase* results,
	unsigned char componentIndex,
	fiftyoneDegreesException* exception) {
	if (walkLog != nullptr) {
		walkLog->push_back((int)componentIndex);
	}
	walkMethod(results, componentIndex, exception);
}

/** Makes the results record each walk in the log given. */
void countWalks(fiftyoneDegreesResultsIpi* results, std::vector<int>& log) {
	walkMethod = results->b.evaluate;
	walkLog = &log;
	results->b.evaluate = countWalk;
}

/** Populates the results for the address, failing the test on an error. */
void populate(fiftyoneDegreesResultsIpi* results, const char* address) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		address,
		strlen(address),
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
}

/** The values of a property with the separator, or why it has none. */
std::string describe(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const bool hasValues = fiftyoneDegreesResultsIpiGetHasValues(
		results,
		requiredPropertyIndex,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	if (hasValues == false) {
		const fiftyoneDegreesResultsNoValueReason reason =
			fiftyoneDegreesResultsIpiGetNoValueReason(
				results,
				requiredPropertyIndex,
				exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		return "no value, reason " + std::to_string((int)reason);
	}
	std::vector<char> buffer(16384, '\0');
	fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
		results,
		requiredPropertyIndex,
		buffer.data(),
		buffer.size(),
		"|",
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(
		buffer.begin(),
		std::find(buffer.begin(), buffer.end(), '\0'));
}

fiftyoneDegreesDataSetIpi* dataSetOf(fiftyoneDegreesResultsIpi* results) {
	return (fiftyoneDegreesDataSetIpi*)results->b.dataSet;
}

int requiredCount(fiftyoneDegreesResultsIpi* results) {
	return (int)dataSetOf(results)->b.b.available->count;
}

/** The name of the property at the index, in the data set the results hold. */
std::string nameOf(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex) {
	const fiftyoneDegreesString* name =
		fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
			dataSetOf(results)->b.b.available,
			requiredPropertyIndex);
	return name == nullptr ? std::string() : std::string(&name->value);
}

int componentOf(fiftyoneDegreesResultsIpi* results, int requiredPropertyIndex) {
	return (int)dataSetOf(results)->b.b.available->items[
		requiredPropertyIndex].componentIndex;
}

/** The first property on another component, or -1 if there is none. */
int propertyOnAnotherComponent(
	fiftyoneDegreesResultsIpi* results,
	int componentIndex) {
	for (int i = 0; i < requiredCount(results); i++) {
		if (componentOf(results, i) != componentIndex) {
			return i;
		}
	}
	return -1;
}

/** Every property's values, read in the order given. */
std::map<int, std::string> readAll(
	fiftyoneDegreesResultsIpi* results,
	const std::vector<int>& order) {
	std::map<int, std::string> values;
	for (int index : order) {
		values[index] = describe(results, index);
	}
	return values;
}

}

/**
 * Tests on a data file with several components, so that reading one
 * component's property can be told apart from walking every graph.
 */
class IpiFirstReadTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath.empty();
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No IP Intelligence data file was found.";
		}
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
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
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;
};

/**
 * Populating the results walks no graph. A read walks the graph of the
 * property's component once, and populating the results again makes every
 * component wait for its first read again.
 */
TEST_F(IpiFirstReadTests, Read_WalksOnlyTheComponentRead) {
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, firstReadAddresses[0]);
	EXPECT_TRUE(walks.empty()) << "Populating the results walked a graph.";

	const int first = 0;
	const int firstComponent = componentOf(results, first);
	const int second = propertyOnAnotherComponent(results, firstComponent);
	if (second < 0) {
		fiftyoneDegreesResultsIpiFree(results);
		GTEST_SKIP() << "The data file has properties on one component only.";
	}
	const int secondComponent = componentOf(results, second);

	describe(results, first);
	EXPECT_EQ(std::vector<int>({ firstComponent }), walks);
	describe(results, first);
	EXPECT_EQ(std::vector<int>({ firstComponent }), walks) <<
		"A second read of the property walked its graph again.";
	describe(results, second);
	EXPECT_EQ(std::vector<int>({ firstComponent, secondComponent }), walks);

	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsEvaluateAll(&results->b, exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	std::vector<int> walked = walks;
	std::sort(walked.begin(), walked.end());
	EXPECT_EQ(walked.end(), std::unique(walked.begin(), walked.end())) <<
		"A component was walked twice.";
	EXPECT_EQ(
		(size_t)dataSetOf(results)->componentsAvailableCount,
		walks.size()) << "Not every available component was walked once.";
	const size_t after = walks.size();
	for (int i = 0; i < requiredCount(results); i++) {
		describe(results, i);
	}
	EXPECT_EQ(after, walks.size()) <<
		"A read after every component was walked walked a graph.";

	populate(results, firstReadAddresses[1]);
	walks.clear();
	describe(results, first);
	EXPECT_EQ(std::vector<int>({ firstComponent }), walks) <<
		"Populating the results again did not make the component wait for "
		"its first read.";
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * Reading every property gives what reading them after walking every graph
 * gives, in any order.
 */
TEST_F(IpiFirstReadTests, ReadEverything_MatchesWalkingEveryGraphFirst) {
	for (const char* address : firstReadAddresses) {
		fiftyoneDegreesResultsIpi* walkedFirst =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpi* forward =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpi* backward =
			fiftyoneDegreesResultsIpiCreate(&manager);
		populate(walkedFirst, address);
		populate(forward, address);
		populate(backward, address);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&walkedFirst->b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		std::vector<int> order;
		for (int i = 0; i < requiredCount(forward); i++) {
			order.push_back(i);
		}
		const std::map<int, std::string> expected =
			readAll(walkedFirst, order);
		EXPECT_EQ(expected, readAll(forward, order)) << address;
		std::reverse(order.begin(), order.end());
		EXPECT_EQ(expected, readAll(backward, order)) << address;
		fiftyoneDegreesResultsIpiFree(walkedFirst);
		fiftyoneDegreesResultsIpiFree(forward);
		fiftyoneDegreesResultsIpiFree(backward);
	}
}

/**
 * Tests on the small data file in the data submodule, whose first required
 * property is renamed in a copy so that a reload moves every other property.
 */
class IpiFirstReadReloadTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		const std::string path =
			GetFilePath(_dataFolderName, firstReadSmallFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << firstReadSmallFileName <<
				"' was not found. Run data/get-data-files.sh or .ps1 to "
				"download it.";
		}
		FILE* file = fopen(path.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
		fseek(file, 0, SEEK_END);
		good.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(good.data(), 1, good.size(), file);
		fclose(file);
		ASSERT_EQ(good.size(), bytes) << "Could not read '" << path << "'.";
		ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(good);
		std::vector<unsigned char>().swap(renamed);
		walkLog = nullptr;
		walkMethod = nullptr;
		Base::TearDown();
	}

protected:
	std::vector<unsigned char> good;
	std::vector<unsigned char> renamed;

	/**
	 * Copies the data file with the first required property renamed. The name
	 * is changed in place, so the layout of the file is the same, and it
	 * sorts after every other name, so the reload moves every other property
	 * down by one.
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
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		const fiftyoneDegreesPropertyAvailable* first =
			&dataSet->b.b.available->items[0];
		const char firstCharacter = (&((const fiftyoneDegreesString*)
			first->name.data.ptr)->value)[0];
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			first->propertyIndex,
			&item,
			exception);
		const bool found =
			property != nullptr && FIFTYONE_DEGREES_EXCEPTION_OKAY;
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
		ASSERT_LT(position, good.size());
		ASSERT_EQ(firstCharacter, (char)good[position]) <<
			"The name of the first property is not where the header and the "
			"property say it is.";
		renamed = good;
		// Sorts after every letter, and no property name has it.
		renamed[position] = '~';
	}
};

/**
 * Results populated before a reload walk their graphs at the first read
 * after it, against the data set they hold, and so read what they would
 * have read before the reload, where the reloaded data set would not.
 */
TEST_F(IpiFirstReadReloadTests, ReadAfterReload_WalksTheDataSetTheResultsHold) {
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

	fiftyoneDegreesResultsIpi* expected =
		fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpi* held =
		fiftyoneDegreesResultsIpiCreate(&manager);
	std::vector<int> walks;
	countWalks(held, walks);
	populate(expected, firstReadAddresses[1]);
	populate(held, firstReadAddresses[1]);
	fiftyoneDegreesResultsEvaluateAll(&expected->b, exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	std::vector<int> order;
	for (int i = 0; i < requiredCount(expected); i++) {
		order.push_back(i);
	}
	const std::map<int, std::string> before = readAll(expected, order);
	fiftyoneDegreesResultsIpiFree(expected);

	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiReloadManagerFromMemory(
			&manager,
			renamed.data(),
			(fiftyoneDegreesFileOffset)renamed.size(),
			exception));
	EXPECT_TRUE(walks.empty()) << "The held results walked before a read.";

	EXPECT_EQ(before, readAll(held, order));
	EXPECT_FALSE(walks.empty()) << "The read after the reload walked nothing.";

	// The reload keeps every index on its property, so the renamed property
	// at index 0 reads nothing from the reloaded data set, where the held
	// results read its value. The reloaded data set still walks a graph only
	// at the first read.
	std::vector<int> reloadedWalks;
	fiftyoneDegreesResultsIpi* reloaded =
		fiftyoneDegreesResultsIpiCreate(&manager);
	countWalks(reloaded, reloadedWalks);
	populate(reloaded, firstReadAddresses[1]);
	EXPECT_TRUE(reloadedWalks.empty()) <<
		"The reloaded data set walked before a read.";
	EXPECT_EQ(nameOf(held, 0), nameOf(reloaded, 0));
	EXPECT_EQ(nameOf(held, 1), nameOf(reloaded, 1));
	const std::map<int, std::string> after = readAll(reloaded, { 0, 1 });
	EXPECT_NE(before.at(0), after.at(0)) <<
		"The renamed property must read nothing after the reload.";
	EXPECT_EQ(before.at(1), after.at(1));
	EXPECT_FALSE(reloadedWalks.empty()) <<
		"The read after the reload walked nothing.";
	fiftyoneDegreesResultsIpiFree(reloaded);
	fiftyoneDegreesResultsIpiFree(held);
	fiftyoneDegreesResourceManagerFree(&manager);
}

namespace {

/** Starting value of the fingerprint, from the 64 bit FNV-1a hash. */
const uint64_t fingerprintOffsetBasis = 14695981039346656037ULL;

/** Multiplier of the fingerprint, from the 64 bit FNV-1a hash. */
const uint64_t fingerprintPrime = 1099511628211ULL;

/** Significant digits written for a double, enough to tell any two apart. */
const int jsonDoubleDigits = 17;

/** Characters shown either side of the first difference between two texts. */
const size_t differenceContextCharacters = 80;

/** A fingerprint of the text, so that runs can be compared from their logs. */
uint64_t fingerprint(const std::string& text) {
	uint64_t hash = fingerprintOffsetBasis;
	for (const char c : text) {
		hash ^= (unsigned char)c;
		hash *= fingerprintPrime;
	}
	return hash;
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

void writeJson(std::ostringstream& json, const std::string& text) {
	json << '"';
	for (const char c : text) {
		if (c == '"' || c == '\\') {
			json << '\\';
		}
		json << c;
	}
	json << '"';
}

void writeJson(std::ostringstream& json, bool value) {
	json << (value ? "true" : "false");
}

void writeJson(std::ostringstream& json, int value) {
	json << value;
}

void writeJson(std::ostringstream& json, double value) {
	json << value;
}

/** Writes the weighted values a getter gives, or why it gives none. */
template <typename T>
void writeJson(
	std::ostringstream& json,
	const FiftyoneDegrees::Common::Value<std::vector<
		FiftyoneDegrees::IpIntelligence::WeightedValue<T>>>& value) {
	if (value.hasValue() == false) {
		const char* message = value.getNoValueMessage();
		json << "{\"noValueReason\":" << (int)value.getNoValueReason() <<
			",\"noValueMessage\":";
		writeJson(json, std::string(message == nullptr ? "" : message));
		json << "}";
		return;
	}
	json << "[";
	const char* separator = "";
	for (const auto& item : value.getValue()) {
		json << separator << "{\"value\":";
		writeJson(json, item.getValue());
		json << ",\"weight\":" << item.getRawWeight() << "}";
		separator = ",";
	}
	json << "]";
}

/**
 * Reads a property through the getter of ResultsIpi that its type needs, as
 * the languages do to build JSON. Returns false if the getter threw, with
 * what it threw as the JSON.
 */
bool readAsJson(
	FiftyoneDegrees::IpIntelligence::ResultsIpi* results,
	const std::string& name,
	const std::string& type,
	std::string& text) {
	std::ostringstream json;
	json.precision(jsonDoubleDigits);
	try {
		if (type == "bool" || type == "weightedbool") {
			writeJson(json, results->getValuesAsWeightedBoolList(name));
		}
		else if (type == "int" || type == "weightedint") {
			writeJson(json, results->getValuesAsWeightedIntegerList(name));
		}
		else if (type == "double" || type == "single" ||
			type == "weighteddouble" || type == "weightedsingle") {
			writeJson(json, results->getValuesAsWeightedDoubleList(name));
		}
		else if (type == "wkb") {
			writeJson(json, results->getValuesAsWeightedWKTStringList(
				name,
				fiftyoneDegreesDefaultWktDecimalPlaces));
		}
		else {
			// Strings and the reduced geometries, which the string list writes
			// as text from the type they are stored as.
			writeJson(json, results->getValuesAsWeightedStringList(name));
		}
	}
	catch (const std::exception& e) {
		json.str("");
		json << "{\"exception\":";
		writeJson(json, std::string(e.what()));
		json << "}";
		text = json.str();
		return false;
	}
	text = json.str();
	return true;
}

/** The name and type of each property the results make available. */
std::vector<std::pair<std::string, std::string>> namesAndTypes(
	FiftyoneDegrees::IpIntelligence::EngineIpi& engine,
	FiftyoneDegrees::IpIntelligence::ResultsIpi* results) {
	std::vector<std::pair<std::string, std::string>> properties;
	std::unique_ptr<FiftyoneDegrees::Common::Collection<
		std::string,
		FiftyoneDegrees::Common::PropertyMetaData>> metaData(
			engine.getMetaData()->getProperties());
	for (const std::string& name : results->getProperties()) {
		std::unique_ptr<FiftyoneDegrees::Common::PropertyMetaData> property(
			metaData->getByKey(name));
		properties.emplace_back(
			name,
			property == nullptr ? std::string() : property->getType());
	}
	return properties;
}

/**
 * The JSON of every property, read in the order given, forwards or in
 * reverse, and written in the order of the names. The name of each property
 * whose getter threw is added to failed, with what it threw.
 */
std::string toJson(
	FiftyoneDegrees::IpIntelligence::ResultsIpi* results,
	const std::vector<std::pair<std::string, std::string>>& properties,
	bool reverse,
	std::vector<std::string>& failed) {
	std::vector<std::string> read(properties.size());
	for (size_t n = 0; n < properties.size(); n++) {
		const size_t i = reverse ? properties.size() - 1 - n : n;
		if (readAsJson(
			results,
			properties[i].first,
			properties[i].second,
			read[i]) == false) {
			failed.push_back(properties[i].first + " (" +
				properties[i].second + ") " + read[i]);
		}
	}
	std::string json = "{";
	for (size_t i = 0; i < properties.size(); i++) {
		json += (i == 0 ? "\"" : ",\"") + properties[i].first + "\":" +
			read[i];
	}
	return json + "}";
}

}

/**
 * Tests through the C++ classes that the languages build JSON with, which read
 * every property through the getters of ResultsIpi.
 */
class IpiFirstReadJsonTests : public Base {
protected:
	/** Walks every graph of the results, as reading every property would. */
	static void walkEveryGraph(
		FiftyoneDegrees::IpIntelligence::ResultsIpi* results) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsEvaluateAll(&results->results->b, exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}
};

/**
 * The JSON of every property is what it is when every graph is walked before
 * the first read, whether the properties are read forwards or in reverse. The
 * fingerprint of each address's JSON is written out, so that the JSON of
 * different builds can be compared.
 */
TEST_F(IpiFirstReadJsonTests, Json_MatchesWalkingEveryGraphFirst) {
	std::string dataFilePath;
	for (int i = 0;
		i < _IpiFileNamesLength && dataFilePath.empty();
		i++) {
		dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
	}
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "No IP Intelligence data file was found.";
	}
	FiftyoneDegrees::IpIntelligence::ConfigIpi config;
	config.setBalanced();
	FiftyoneDegrees::Common::RequiredPropertiesConfig required;
	FiftyoneDegrees::IpIntelligence::EngineIpi engine(
		dataFilePath,
		&config,
		&required);
	for (const char* address : firstReadAddresses) {
		std::unique_ptr<FiftyoneDegrees::IpIntelligence::ResultsIpi> walked(
			engine.process(address));
		std::unique_ptr<FiftyoneDegrees::IpIntelligence::ResultsIpi> forward(
			engine.process(address));
		std::unique_ptr<FiftyoneDegrees::IpIntelligence::ResultsIpi> reverse(
			engine.process(address));
		walkEveryGraph(walked.get());
		const std::vector<std::pair<std::string, std::string>> properties =
			namesAndTypes(engine, forward.get());
		ASSERT_FALSE(properties.empty()) << address;
		std::vector<std::string> failed;
		const std::string expected =
			toJson(walked.get(), properties, false, failed);
		EXPECT_EQ(std::string(), firstDifference(
			expected,
			toJson(forward.get(), properties, false, failed))) << address <<
			" read forwards";
		EXPECT_EQ(std::string(), firstDifference(
			expected,
			toJson(reverse.get(), properties, true, failed))) << address <<
			" read in reverse";
		EXPECT_TRUE(failed.empty()) << address << ", the getter threw for " <<
			failed.size() << " reads, the first " << failed.front();
		std::printf(
			"JSON of %s, %u properties, %u characters, fingerprint %016llx\n",
			address,
			(unsigned int)properties.size(),
			(unsigned int)expected.size(),
			(unsigned long long)fingerprint(expected));
		std::fflush(stdout);
	}
}
