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
#include "../src/common-cxx/tests/Base.hpp"
#include "../src/EngineIpi.hpp"
#include "../src/fiftyone.h"

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
