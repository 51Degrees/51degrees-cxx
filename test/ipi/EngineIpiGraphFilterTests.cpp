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

#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using namespace std;
// Aliases rather than open namespaces, because the C synonym typedefs from
// fiftyone.h share their short names with the C++ classes.
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

// A public address from the evidence file that ships with the test data.
static const char* graphFilterIpAddress = "50.154.29.201";
static const unsigned char graphFilterIpBytes[] = { 50, 154, 29, 201 };

// The engine marks a component that was not evaluated with UINT32_MAX, its
// internal NULL_PROFILE_OFFSET sentinel, which is not exported.
static const uint32_t graphFilterNullOffset = UINT32_MAX;

/**
 * C level tests for the ...ForProperties twins. Every property is required
 * so every component in the file is available, and the tests reason about
 * result slots, one per available component in component order. The C types
 * are fully qualified because the C++ classes of the same short names are in
 * scope.
 *
 * A property that is mandatory with a default value reads as that default
 * when its component produced no profile, exactly as for an unmatched
 * component today, so the value assertions use properties without one.
 */
class IpiGraphFilterCTests : public Base {
public:
	void SetUp() {
		Base::SetUp();
		EXCEPTION_CREATE;
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath == "";
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		fiftyoneDegreesIpiInitManagerFromFile(
			&manager,
			&config,
			&properties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
	}
	void TearDown() {
		fiftyoneDegreesResourceManagerFree(&manager);
		Base::TearDown();
	}
protected:
	string dataFilePath = "";
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiDefaultConfig;
	fiftyoneDegreesResourceManager manager;

	static fiftyoneDegreesDataSetIpi* dataSetOf(
		fiftyoneDegreesResultsIpi* results) {
		return (fiftyoneDegreesDataSetIpi*)results->b.dataSet;
	}

	// True when the property is mandatory with a default value.
	static bool mandatoryWithDefault(
		fiftyoneDegreesResultsIpi* results,
		int requiredIndex) {
		EXCEPTION_CREATE;
		fiftyoneDegreesDataSetIpi* dataSet = dataSetOf(results);
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		int propertyIndex =
			fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
				dataSet->b.b.available,
				requiredIndex);
		fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			propertyIndex,
			&item,
			exception);
		EXCEPTION_THROW;
		bool result = property != NULL &&
			property->isMandatory &&
			property->defaultValueIndex != UINT32_MAX;
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		return result;
	}

	// First required property without a mandatory default, on a component
	// other than the one given, or on any component when that is -1.
	// Returns -1 when there is none.
	static int optionalProperty(
		fiftyoneDegreesResultsIpi* results,
		int notOnComponent) {
		fiftyoneDegreesPropertiesAvailable* available =
			dataSetOf(results)->b.b.available;
		for (uint32_t i = 0; i < available->count; i++) {
			if ((notOnComponent < 0 ||
				available->items[i].componentIndex != notOnComponent) &&
				mandatoryWithDefault(results, (int)i) == false) {
				return (int)i;
			}
		}
		return -1;
	}

	// First required property on a component other than the one given,
	// whether or not it has a mandatory default. Returns -1 when every
	// property is on that component.
	static int propertyOnAnotherComponent(
		fiftyoneDegreesResultsIpi* results,
		int notOnComponent) {
		fiftyoneDegreesPropertiesAvailable* available =
			dataSetOf(results)->b.b.available;
		for (uint32_t i = 0; i < available->count; i++) {
			if (available->items[i].componentIndex != notOnComponent) {
				return (int)i;
			}
		}
		return -1;
	}

	// The slot in the results that holds the component of the required
	// property index, following the positional mapping over available
	// components.
	static int slotForProperty(
		fiftyoneDegreesResultsIpi* results,
		int requiredIndex) {
		fiftyoneDegreesDataSetIpi* dataSet = dataSetOf(results);
		unsigned char component =
			dataSet->b.b.available->items[requiredIndex].componentIndex;
		int slot = 0;
		for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
			if (dataSet->componentsAvailable[c]) {
				if (c == component) {
					return slot;
				}
				slot++;
			}
		}
		return -1;
	}

	void detectString(
		fiftyoneDegreesResultsIpi* results,
		const int* indexes,
		int count) {
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
			results,
			graphFilterIpAddress,
			strlen(graphFilterIpAddress),
			indexes,
			count,
			exception);
		EXCEPTION_THROW;
	}

	void expectNullProfile(fiftyoneDegreesResultsIpi* results, int index) {
		EXCEPTION_CREATE;
		EXPECT_FALSE(fiftyoneDegreesResultsIpiGetHasValues(
			results, index, exception)) <<
			"Required property " << index << " should have no value.";
		EXCEPTION_THROW;
		EXPECT_EQ(
			FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE,
			fiftyoneDegreesResultsIpiGetNoValueReason(
				results, index, exception));
		EXCEPTION_THROW;
	}
};

TEST_F(IpiGraphFilterCTests, NullIndexesMatchesUnfilteredDetection) {
	EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpi* all = fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpi* filtered =
		fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpiFromIpAddressString(
		all, graphFilterIpAddress, strlen(graphFilterIpAddress), exception);
	EXCEPTION_THROW;
	detectString(filtered, NULL, -1);
	ASSERT_EQ(all->count, filtered->count);
	for (uint32_t i = 0; i < all->count; i++) {
		EXPECT_EQ(
			all->items[i].graphResult.rawOffset,
			filtered->items[i].graphResult.rawOffset) << "Slot " << i;
	}
	fiftyoneDegreesResultsIpiFree(all);
	fiftyoneDegreesResultsIpiFree(filtered);
}

TEST_F(IpiGraphFilterCTests, EmptyIndexesEvaluatesNoGraph) {
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	int none[] = { 0 };
	detectString(results, none, 0);
	EXPECT_EQ(dataSetOf(results)->componentsAvailableCount, results->count) <<
		"The result shape must not change when graphs are skipped.";
	for (uint32_t i = 0; i < results->count; i++) {
		EXPECT_EQ(graphFilterNullOffset, results->items[i].graphResult.rawOffset)
			<< "Slot " << i << " should hold a null profile.";
	}
	int optional = optionalProperty(results, -1);
	if (optional >= 0) {
		expectNullProfile(results, optional);
	}
	fiftyoneDegreesResultsIpiFree(results);
}

TEST_F(IpiGraphFilterCTests, OnePropertyEvaluatesOnlyItsComponent) {
	fiftyoneDegreesResultsIpi* all = fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpi* filtered =
		fiftyoneDegreesResultsIpiCreate(&manager);
	int first = 0;
	int firstComponent =
		dataSetOf(all)->b.b.available->items[first].componentIndex;
	int second = propertyOnAnotherComponent(all, firstComponent);
	if (second < 0) {
		fiftyoneDegreesResultsIpiFree(all);
		fiftyoneDegreesResultsIpiFree(filtered);
		GTEST_SKIP() << "The data file has properties on one component only.";
	}
	detectString(all, NULL, -1);
	int indexes[] = { first };
	detectString(filtered, indexes, 1);
	ASSERT_EQ(all->count, filtered->count);
	int keptSlot = slotForProperty(all, first);
	int skippedSlot = slotForProperty(all, second);
	ASSERT_GE(keptSlot, 0);
	ASSERT_GE(skippedSlot, 0);
	ASSERT_NE(keptSlot, skippedSlot);
	ASSERT_NE(graphFilterNullOffset, all->items[skippedSlot].graphResult.rawOffset)
		<< "The address must give the second component a profile when "
		"unfiltered, or the check below proves nothing.";
	EXPECT_EQ(
		all->items[keptSlot].graphResult.rawOffset,
		filtered->items[keptSlot].graphResult.rawOffset) <<
		"The evaluated component must match the unfiltered detection.";
	EXPECT_EQ(
		graphFilterNullOffset,
		filtered->items[skippedSlot].graphResult.rawOffset) <<
		"The skipped component must hold a null profile.";
	// A property with a mandatory default reads as that default, so only a
	// property without one can show the null profile reason.
	int optional = optionalProperty(filtered, firstComponent);
	if (optional >= 0) {
		expectNullProfile(filtered, optional);
	}
	fiftyoneDegreesResultsIpiFree(all);
	fiftyoneDegreesResultsIpiFree(filtered);
}

TEST_F(IpiGraphFilterCTests, BadIndexesAreIgnored) {
	fiftyoneDegreesResultsIpi* all = fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpi* filtered =
		fiftyoneDegreesResultsIpiCreate(&manager);
	detectString(all, NULL, -1);
	int indexes[] = {
		-1, 0, (int)dataSetOf(all)->b.b.available->count, 100000 };
	detectString(filtered, indexes, 4);
	int slot = slotForProperty(all, 0);
	ASSERT_GE(slot, 0);
	EXPECT_EQ(
		all->items[slot].graphResult.rawOffset,
		filtered->items[slot].graphResult.rawOffset);
	fiftyoneDegreesResultsIpiFree(all);
	fiftyoneDegreesResultsIpiFree(filtered);
}

TEST_F(IpiGraphFilterCTests, ByteArrayTwinMatchesStringTwin) {
	EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpi* fromString =
		fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpi* fromBytes =
		fiftyoneDegreesResultsIpiCreate(&manager);
	int indexes[] = { 0 };
	detectString(fromString, indexes, 1);
	fiftyoneDegreesResultsIpiFromIpAddressForProperties(
		fromBytes,
		graphFilterIpBytes,
		sizeof(graphFilterIpBytes),
		FIFTYONE_DEGREES_IP_TYPE_IPV4,
		indexes,
		1,
		exception);
	EXCEPTION_THROW;
	ASSERT_EQ(fromString->count, fromBytes->count);
	for (uint32_t i = 0; i < fromString->count; i++) {
		EXPECT_EQ(
			fromString->items[i].graphResult.rawOffset,
			fromBytes->items[i].graphResult.rawOffset) << "Slot " << i;
	}
	fiftyoneDegreesResultsIpiFree(fromString);
	fiftyoneDegreesResultsIpiFree(fromBytes);
}

/**
 * C++ tests for the EngineIpi::process overloads and getRequiredProperties.
 */
class EngineIpiGraphFilterTests : public Base {
public:
	void SetUp() {
		Base::SetUp();
		string dataFilePath = "";
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath == "";
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		config = new Ipi::ConfigIpi();
		// Balanced rather than the in memory default, so a test does not
		// hold the whole data file, which is several gigabytes for the
		// enterprise file.
		config->setBalanced();
		properties = new Common::RequiredPropertiesConfig();
		engine = new Ipi::EngineIpi(dataFilePath, config, properties);
	}
	void TearDown() {
		delete engine;
		delete properties;
		delete config;
		Base::TearDown();
	}
protected:
	Ipi::ConfigIpi* config = nullptr;
	Common::RequiredPropertiesConfig* properties = nullptr;
	Ipi::EngineIpi* engine = nullptr;

	// The first evidence key the engine accepts, used to hand it an address.
	string evidenceKey() {
		const vector<string>* keys = engine->getKeys();
		return keys->empty() ? string("query.client-ip") : keys->at(0);
	}

	// True when the property is mandatory with a default value, which reads
	// as that default when its component produced no profile.
	bool mandatoryWithDefault(const string &name) {
		unique_ptr<Common::Collection<string, Common::PropertyMetaData>> allProperties(
			engine->getMetaData()->getProperties());
		unique_ptr<Common::PropertyMetaData> property(
			allProperties->getByKey(name));
		return property != nullptr &&
			property->getIsMandatory() &&
			property->getDefaultValue().empty() == false;
	}

	// Every property must agree between two results, both in whether it has
	// a value and in the value itself.
	void expectSameValues(Ipi::ResultsIpi* expected, Ipi::ResultsIpi* actual) {
		vector<string> names = engine->getRequiredProperties();
		for (size_t i = 0; i < names.size(); i++) {
			Common::Value<string> e = expected->getValueAsString(names[i]);
			Common::Value<string> a = actual->getValueAsString(names[i]);
			EXPECT_EQ(e.hasValue(), a.hasValue()) << names[i];
			if (e.hasValue() && a.hasValue()) {
				EXPECT_EQ(e.getValue(), a.getValue()) << names[i];
			}
		}
	}
};

TEST_F(EngineIpiGraphFilterTests, RequiredPropertiesAreInIndexOrder) {
	vector<string> names = engine->getRequiredProperties();
	unique_ptr<Ipi::ResultsIpi> results(engine->process(graphFilterIpAddress));
	vector<string> fromResults = results->getProperties();
	ASSERT_EQ(fromResults.size(), names.size());
	for (size_t i = 0; i < names.size(); i++) {
		EXPECT_EQ(fromResults[i], names[i]) <<
			"Name at required property index " << i << " differs.";
	}
	EXPECT_GT(names.size(), 0u);
}

TEST_F(EngineIpiGraphFilterTests, NullIndexesGiveEveryValue) {
	unique_ptr<Ipi::ResultsIpi> all(engine->process(graphFilterIpAddress));
	unique_ptr<Ipi::ResultsIpi> filtered(
		engine->process(graphFilterIpAddress, nullptr, -1));
	expectSameValues(all.get(), filtered.get());
}

TEST_F(EngineIpiGraphFilterTests, EmptyIndexesGiveNoValue) {
	vector<string> names = engine->getRequiredProperties();
	int none[] = { 0 };
	unique_ptr<Ipi::ResultsIpi> all(engine->process(graphFilterIpAddress));
	unique_ptr<Ipi::ResultsIpi> results(
		engine->process(graphFilterIpAddress, none, 0));
	int changed = 0;
	for (size_t i = 0; i < names.size(); i++) {
		Common::Value<string> unfiltered = all->getValueAsString(names[i]);
		Common::Value<string> value = results->getValueAsString(names[i]);
		if (unfiltered.hasValue() != value.hasValue() ||
			(unfiltered.hasValue() &&
				unfiltered.getValue() != value.getValue())) {
			changed++;
		}
		if (mandatoryWithDefault(names[i])) {
			// The default stands in, as for a component that produced no
			// profile.
			continue;
		}
		EXPECT_FALSE(value.hasValue()) << names[i];
		EXPECT_EQ(
			FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE,
			value.getNoValueReason()) << names[i];
	}
	// Properties with a mandatory default read as that default, so compare
	// with the unfiltered detection to show the graphs were skipped.
	EXPECT_GT(changed, 0) << "No value changed when every graph was skipped.";
}

TEST_F(EngineIpiGraphFilterTests, EvidenceOverloadMatchesStringOverload) {
	int indexes[] = { 0 };
	Ipi::EvidenceIpi evidence;
	evidence[evidenceKey()] = graphFilterIpAddress;
	unique_ptr<Ipi::ResultsIpi> fromEvidence(
		engine->process(&evidence, indexes, 1));
	unique_ptr<Ipi::ResultsIpi> fromString(
		engine->process(graphFilterIpAddress, indexes, 1));
	expectSameValues(fromString.get(), fromEvidence.get());
}

TEST_F(EngineIpiGraphFilterTests, ByteArrayOverloadMatchesStringOverload) {
	int indexes[] = { 0 };
	unsigned char bytes[] = { 50, 154, 29, 201 };
	unique_ptr<Ipi::ResultsIpi> fromBytes(engine->process(
		bytes, sizeof(bytes), FIFTYONE_DEGREES_IP_TYPE_IPV4, indexes, 1));
	unique_ptr<Ipi::ResultsIpi> fromString(
		engine->process(graphFilterIpAddress, indexes, 1));
	expectSameValues(fromString.get(), fromBytes.get());
}

/**
 * A manager over the data file with the required properties given, or every
 * property when there are none, which is freed however the test ends.
 */
class RequiredOnlyManager {
public:
	RequiredOnlyManager(
		const string& dataFilePath,
		fiftyoneDegreesConfigIpi config,
		const char* required) {
		EXCEPTION_CREATE;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				dataFilePath.c_str(),
				exception);
		if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
			throw Common::StatusCodeException(status);
		}
		EXCEPTION_THROW;
	}
	~RequiredOnlyManager() {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
	fiftyoneDegreesResourceManager manager;
};

/**
 * Tests for the required components only setting. The properties of the
 * component holding RegisteredCountry are required, so the graphs and profiles
 * of the others stay in the file. Every answer is compared with a data set that
 * has the same required properties without the setting, using the low memory
 * configuration so that a test does not hold the whole data file in memory.
 */
class IpiRequiredComponentsOnlyTests : public Base {
public:
	void SetUp() {
		Base::SetUp();
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath == "";
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		RequiredOnlyManager all(
			dataFilePath,
			fiftyoneDegreesIpiLowMemoryConfig,
			nullptr);
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&all.manager);
		fiftyoneDegreesPropertiesAvailable* available =
			dataSet->b.b.available;
		const int index =
			fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
				available,
				"RegisteredCountry");
		ASSERT_GE(index, 0);
		component = available->items[index].componentIndex;
		for (uint32_t i = 0; i < available->count; i++) {
			const string name = FIFTYONE_DEGREES_STRING(
				fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
					available,
					(int)i));
			allNames.push_back(name);
			if (available->items[i].componentIndex == component) {
				requiredNames.push_back(name);
				required += (required.empty() ? "" : ",") + name;
			}
		}
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		if (requiredNames.size() == allNames.size()) {
			GTEST_SKIP() << "The data file has properties on one component only.";
		}
	}
protected:
	string dataFilePath = "";
	string required = "";
	vector<string> requiredNames;
	vector<string> allNames;
	uint8_t component = 0;

	static fiftyoneDegreesConfigIpi withSetting(
		const fiftyoneDegreesConfigIpi& preset) {
		fiftyoneDegreesConfigIpi config = preset;
		config.requiredComponentsOnly = true;
		return config;
	}

	// Addresses across the IPv4 space, and a few from IPv6.
	static vector<string> addresses() {
		vector<string> result = {
			graphFilterIpAddress,
			"1.1.1.1",
			"8.8.8.8",
			"2001:4860:4860::8888",
			"2606:4700:4700::1111",
			"2a00:1450:4001:81f::200e" };
		for (uint32_t i = 1; i <= 100; i++) {
			const uint32_t x = i * 2654435761u;
			result.push_back(
				to_string(1 + (x >> 24) % 223) + "." +
				to_string((x >> 16) & 255) + "." +
				to_string((x >> 8) & 255) + "." +
				to_string(x & 255));
		}
		return result;
	}

	static void detect(
		fiftyoneDegreesResultsIpi* results,
		const string& address) {
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			address.c_str(),
			address.size(),
			exception);
		EXCEPTION_THROW;
	}

	static string valueOf(
		fiftyoneDegreesResultsIpi* results,
		const string& property,
		vector<char>& buffer) {
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiGetValuesString(
			results,
			property.c_str(),
			buffer.data(),
			buffer.size(),
			"|",
			exception);
		EXCEPTION_THROW;
		return string(buffer.data());
	}

	static void expectSameAnswers(
		fiftyoneDegreesResourceManager* expected,
		fiftyoneDegreesResourceManager* actual,
		const vector<string>& properties) {
		vector<char> buffer(1024 * 1024);
		fiftyoneDegreesResultsIpi* expectedResults =
			fiftyoneDegreesResultsIpiCreate(expected);
		fiftyoneDegreesResultsIpi* actualResults =
			fiftyoneDegreesResultsIpiCreate(actual);
		for (const string& address : addresses()) {
			detect(expectedResults, address);
			detect(actualResults, address);
			for (const string& property : properties) {
				EXPECT_EQ(
					valueOf(expectedResults, property, buffer),
					valueOf(actualResults, property, buffer)) <<
					property << " of " << address;
			}
		}
		fiftyoneDegreesResultsIpiFree(expectedResults);
		fiftyoneDegreesResultsIpiFree(actualResults);
	}

	static string profilesTypeOf(fiftyoneDegreesResourceManager* manager) {
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(manager);
		const string type = dataSet->profiles->typeName;
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		return type;
	}
};

TEST_F(IpiRequiredComponentsOnlyTests, EveryPresetLeavesTheSettingOff) {
	EXPECT_FALSE(fiftyoneDegreesIpiInMemoryConfig.requiredComponentsOnly);
	EXPECT_FALSE(fiftyoneDegreesIpiHighPerformanceConfig.requiredComponentsOnly);
	EXPECT_FALSE(fiftyoneDegreesIpiLowMemoryConfig.requiredComponentsOnly);
	EXPECT_FALSE(fiftyoneDegreesIpiBalancedConfig.requiredComponentsOnly);
	EXPECT_FALSE(fiftyoneDegreesIpiBalancedTempConfig.requiredComponentsOnly);
	EXPECT_FALSE(fiftyoneDegreesIpiDefaultConfig.requiredComponentsOnly);
}

TEST_F(IpiRequiredComponentsOnlyTests, ChoosingAPresetKeepsTheSetting) {
	Ipi::ConfigIpi config;
	EXPECT_FALSE(config.getRequiredComponentsOnly());
	config.setRequiredComponentsOnly(true);
	config.setHighPerformance();
	EXPECT_TRUE(config.getRequiredComponentsOnly());
	EXPECT_TRUE(config.getConfig().requiredComponentsOnly);
	config.setRequiredComponentsOnly(false);
	EXPECT_FALSE(config.getRequiredComponentsOnly());
}

TEST_F(IpiRequiredComponentsOnlyTests, AnswersMatchWithoutTheSetting) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		required.c_str());
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		required.c_str());
	EXPECT_EQ("CollectionRequiredProfiles", profilesTypeOf(&actual.manager));
	expectSameAnswers(&expected.manager, &actual.manager, requiredNames);
}

TEST_F(IpiRequiredComponentsOnlyTests, HoldsLessThanTheDataFile) {
	EXCEPTION_CREATE;
	fiftyoneDegreesConfigIpi config =
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig);
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = required.c_str();
	const size_t size = fiftyoneDegreesIpiSizeManagerFromFile(
		&config,
		&properties,
		dataFilePath.c_str(),
		exception);
	EXCEPTION_THROW;
	EXPECT_GT(size, (size_t)0);
	EXPECT_LT(
		(int64_t)size,
		(int64_t)fiftyoneDegreesFileGetSize(dataFilePath.c_str()));
}

TEST_F(IpiRequiredComponentsOnlyTests, EveryProfileCanStillBeRead) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		required.c_str());
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		required.c_str());
	fiftyoneDegreesDataSetIpi* e =
		fiftyoneDegreesDataSetIpiGet(&expected.manager);
	fiftyoneDegreesDataSetIpi* a =
		fiftyoneDegreesDataSetIpiGet(&actual.manager);
	const uint32_t count = fiftyoneDegreesCollectionGetCount(a->profileOffsets);
	const uint32_t step = count > 1000 ? count / 1000 : 1;
	uint32_t others = 0;
	for (uint32_t i = 0; i < count; i += step) {
		EXCEPTION_CREATE;
		fiftyoneDegreesCollectionItem expectedItem, actualItem;
		fiftyoneDegreesDataReset(&expectedItem.data);
		fiftyoneDegreesDataReset(&actualItem.data);
		const fiftyoneDegreesProfile* expectedProfile =
			fiftyoneDegreesProfileGetByIndex(
				e->profileOffsets, e->profiles, i, &expectedItem, exception);
		EXCEPTION_THROW;
		const fiftyoneDegreesProfile* actualProfile =
			fiftyoneDegreesProfileGetByIndex(
				a->profileOffsets, a->profiles, i, &actualItem, exception);
		EXCEPTION_THROW;
		ASSERT_NE(nullptr, expectedProfile);
		ASSERT_NE(nullptr, actualProfile);
		ASSERT_EQ(
			(uint32_t)expectedProfile->valueCount,
			(uint32_t)actualProfile->valueCount);
		EXPECT_EQ(
			0,
			memcmp(
				expectedProfile,
				actualProfile,
				sizeof(fiftyoneDegreesProfile) +
				expectedProfile->valueCount * sizeof(uint32_t))) <<
			"Profile index " << i;
		if (actualProfile->componentIndex != component) {
			others++;
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(e->profiles, &expectedItem);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(a->profiles, &actualItem);
	}
	EXPECT_GT(others, 0u) << "No profile outside the required component read.";
	fiftyoneDegreesDataSetIpiRelease(e);
	fiftyoneDegreesDataSetIpiRelease(a);
}

TEST_F(IpiRequiredComponentsOnlyTests, EveryGraphCanStillBeEvaluated) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		required.c_str());
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		required.c_str());
	fiftyoneDegreesDataSetIpi* e =
		fiftyoneDegreesDataSetIpiGet(&expected.manager);
	fiftyoneDegreesDataSetIpi* a =
		fiftyoneDegreesDataSetIpiGet(&actual.manager);
	for (const string& address : addresses()) {
		fiftyoneDegreesIpAddress ip;
		ASSERT_TRUE(fiftyoneDegreesIpAddressParse(
			address.c_str(),
			address.c_str() + address.size(),
			&ip));
		for (uint32_t g = 0; g < a->graphsArray->count; g++) {
			EXCEPTION_CREATE;
			const uint8_t id = a->graphsArray->items[g].info.componentId;
			const fiftyoneDegreesIpiCgResult expectedResult =
				fiftyoneDegreesIpiGraphEvaluate(
					e->graphsArray, id, ip, exception);
			EXCEPTION_THROW;
			const fiftyoneDegreesIpiCgResult actualResult =
				fiftyoneDegreesIpiGraphEvaluate(
					a->graphsArray, id, ip, exception);
			EXCEPTION_THROW;
			EXPECT_EQ(expectedResult.rawOffset, actualResult.rawOffset) <<
				"Component " << (int)id << " for " << address;
		}
	}
	fiftyoneDegreesDataSetIpiRelease(e);
	fiftyoneDegreesDataSetIpiRelease(a);
}

TEST_F(IpiRequiredComponentsOnlyTests, OnlyRequiredGraphsAreInMemory) {
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		required.c_str());
	fiftyoneDegreesDataSetIpi* a =
		fiftyoneDegreesDataSetIpiGet(&actual.manager);
	const uint8_t requiredId = ((fiftyoneDegreesComponent*)
		a->componentsList.items[component].data.ptr)->componentId;
	for (uint32_t g = 0; g < a->graphsArray->count; g++) {
		const fiftyoneDegreesIpiCg& graph = a->graphsArray->items[g];
		const string expectedType = graph.info.componentId == requiredId ?
			"CollectionMemory" : "CollectionFile";
		EXPECT_EQ(expectedType, graph.nodes->typeName);
		EXPECT_EQ(expectedType, graph.spans->typeName);
		EXPECT_EQ(expectedType, graph.spanBytes->typeName);
		EXPECT_EQ(expectedType, graph.clusters->typeName);
	}
	fiftyoneDegreesDataSetIpiRelease(a);
}

TEST_F(IpiRequiredComponentsOnlyTests, ReloadKeepsTheSetting) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		required.c_str());
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		required.c_str());
	EXCEPTION_CREATE;
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesIpiReloadManagerFromOriginalFile(
			&actual.manager,
			exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
	EXCEPTION_THROW;
	EXPECT_EQ("CollectionRequiredProfiles", profilesTypeOf(&actual.manager));
	expectSameAnswers(&expected.manager, &actual.manager, requiredNames);
}

TEST_F(IpiRequiredComponentsOnlyTests, EveryPropertyLoadsAsWithoutTheSetting) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		nullptr);
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiHighPerformanceConfig),
		nullptr);
	EXPECT_EQ("CollectionMemory", profilesTypeOf(&actual.manager));
	expectSameAnswers(&expected.manager, &actual.manager, allNames);
}

TEST_F(IpiRequiredComponentsOnlyTests, OtherSettingsStillApply) {
	RequiredOnlyManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		required.c_str());
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiBalancedConfig),
		required.c_str());
	EXPECT_EQ("CollectionCache", profilesTypeOf(&actual.manager));
	expectSameAnswers(&expected.manager, &actual.manager, requiredNames);
}

TEST_F(IpiRequiredComponentsOnlyTests, NoEffectWhenAllInMemory) {
	RequiredOnlyManager actual(
		dataFilePath,
		withSetting(fiftyoneDegreesIpiInMemoryConfig),
		required.c_str());
	EXPECT_EQ("CollectionMemory", profilesTypeOf(&actual.manager));
}

TEST_F(IpiRequiredComponentsOnlyTests, EngineMetaDataMatchesWithoutTheSetting) {
	Common::RequiredPropertiesConfig properties(required);
	Ipi::ConfigIpi expectedConfig;
	expectedConfig.setLowMemory();
	Ipi::ConfigIpi actualConfig;
	actualConfig.setHighPerformance();
	actualConfig.setRequiredComponentsOnly(true);
	Ipi::EngineIpi expected(dataFilePath, &expectedConfig, &properties);
	Ipi::EngineIpi actual(dataFilePath, &actualConfig, &properties);
	using Components = Common::Collection<::byte, Common::ComponentMetaData>;
	using Profiles = Common::Collection<uint32_t, Common::ProfileMetaData>;

	unique_ptr<Components> components(actual.getMetaData()->getComponents());
	for (uint32_t i = 0; i < components->getSize(); i++) {
		unique_ptr<Common::ComponentMetaData> item(components->getByIndex(i));
		unique_ptr<Common::ProfileMetaData> expectedDefault(
			expected.getMetaData()->getDefaultProfileForComponent(item.get()));
		unique_ptr<Common::ProfileMetaData> actualDefault(
			actual.getMetaData()->getDefaultProfileForComponent(item.get()));
		ASSERT_EQ(expectedDefault == nullptr, actualDefault == nullptr);
		if (actualDefault != nullptr) {
			EXPECT_EQ(expectedDefault->getKey(), actualDefault->getKey());
			EXPECT_EQ(
				expectedDefault->getComponentId(),
				actualDefault->getComponentId());
		}
	}

	unique_ptr<Profiles> expectedProfiles(
		expected.getMetaData()->getProfiles());
	unique_ptr<Profiles> actualProfiles(actual.getMetaData()->getProfiles());
	ASSERT_EQ(expectedProfiles->getSize(), actualProfiles->getSize());
	const uint32_t step =
		actualProfiles->getSize() > 1000 ? actualProfiles->getSize() / 1000 : 1;
	for (uint32_t i = 0; i < actualProfiles->getSize(); i += step) {
		unique_ptr<Common::ProfileMetaData> e(expectedProfiles->getByIndex(i));
		unique_ptr<Common::ProfileMetaData> a(actualProfiles->getByIndex(i));
		ASSERT_EQ(e == nullptr, a == nullptr);
		if (a != nullptr) {
			EXPECT_EQ(e->getKey(), a->getKey());
			EXPECT_EQ(e->getComponentId(), a->getComponentId());
		}
	}
}
