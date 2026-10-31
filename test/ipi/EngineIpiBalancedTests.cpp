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
 * @file EngineIpiBalancedTests.cpp
 * @brief Tests the Balanced preset, and how naming required properties decides
 * what it holds in memory.
 *
 * Every answer is compared with the Low Memory preset naming the same
 * properties, which reads everything from the file. The tests use the largest
 * data file they find. The ones needing several components are skipped with a
 * smaller file.
 */

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"
#include "../../src/common/collectionKeyTypes.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;
// Aliases rather than open namespaces, because the C synonym typedefs from
// fiftyone.h share their short names with the C++ classes.
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

// The data file committed to the data submodule, which is always available.
static const char* balancedAsnFileName = "51Degrees-IPIV4AsnIpiV41.ipi";

static const char* balancedMemoryType = "CollectionMemory";
static const char* balancedFileType = "CollectionFile";
static const char* balancedCacheType = "CollectionCache";
static const char* balancedHeldType = "CollectionHeld";

static const size_t balancedBufferBytes = 1024 * 1024;

/**
 * A manager over a data file with the required properties given, or every
 * property when there are none, which is freed however the test ends.
 */
class BalancedManager {
public:
	BalancedManager(
		const string& dataFilePath,
		fiftyoneDegreesConfigIpi config,
		const char* required) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		init(dataFilePath, config, properties);
	}
	BalancedManager(
		const string& dataFilePath,
		fiftyoneDegreesConfigIpi config,
		fiftyoneDegreesPropertiesRequired properties) {
		init(dataFilePath, config, properties);
	}
	~BalancedManager() {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
	fiftyoneDegreesResourceManager manager;
private:
	void init(
		const string& dataFilePath,
		fiftyoneDegreesConfigIpi& config,
		fiftyoneDegreesPropertiesRequired& properties) {
		EXCEPTION_CREATE;
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
};

/**
 * The data set of a manager for the length of a scope.
 */
class BalancedDataSet {
public:
	explicit BalancedDataSet(fiftyoneDegreesResourceManager* manager) {
		dataSet = fiftyoneDegreesDataSetIpiGet(manager);
	}
	~BalancedDataSet() {
		fiftyoneDegreesDataSetIpiRelease(dataSet);
	}
	fiftyoneDegreesDataSetIpi* operator->() const {
		return dataSet;
	}
	fiftyoneDegreesDataSetIpi* dataSet;
};

// Addresses across the IPv4 space, and a few from IPv6.
static vector<string> balancedAddresses() {
	vector<string> result = {
		"50.154.29.201",
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

static void balancedDetect(
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

// Everything a caller reads for one property of the results.
static string balancedAnswer(
	fiftyoneDegreesResultsIpi* results,
	const int index,
	vector<char>& buffer) {
	EXCEPTION_CREATE;
	const bool has = fiftyoneDegreesResultsIpiGetHasValues(
		results,
		index,
		exception);
	EXCEPTION_THROW;
	const size_t added =
		fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
			results,
			index,
			buffer.data(),
			buffer.size(),
			"|",
			exception);
	EXCEPTION_THROW;
	return (has ? "1:" : "0:") + to_string(added) + ":" +
		to_string(hash<string>{}(string(buffer.data())));
}

// The answers for every required property of every address.
static vector<string> balancedAnswers(
	fiftyoneDegreesResourceManager* manager,
	const vector<string>& addresses) {
	vector<char> buffer(balancedBufferBytes);
	vector<string> answers;
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(manager);
	const fiftyoneDegreesDataSetIpi* dataSet =
		(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
	const int count = (int)dataSet->b.b.available->count;
	for (const string& address : addresses) {
		balancedDetect(results, address);
		for (int i = 0; i < count; i++) {
			answers.push_back(balancedAnswer(results, i, buffer));
		}
	}
	fiftyoneDegreesResultsIpiFree(results);
	return answers;
}

/**
 * The types of the collections of a data set which the tests check.
 */
static string balancedTypes(fiftyoneDegreesResourceManager* manager) {
	BalancedDataSet dataSet(manager);
	return string(dataSet->strings->typeName) + " " +
		dataSet->values->typeName + " " +
		dataSet->profiles->typeName + " " +
		dataSet->propertyTypes->typeName + " " +
		dataSet->profileOffsets->typeName;
}

static uint32_t balancedCacheCapacity(const fiftyoneDegreesCollection* cached) {
	return ((const fiftyoneDegreesCollectionCache*)cached->state)
		->cache->capacity;
}

/**
 * Counts and refuses allocations, to check nothing is allocated during a lookup
 * and that everything is freed when an allocation fails.
 */
class BalancedAllocations {
public:
	BalancedAllocations() {
		original = fiftyoneDegreesMalloc;
		originalFree = fiftyoneDegreesFree;
		calls = 0;
		live = 0;
		failAt = 0;
		failedSize = 0;
		fromSize = 0;
		fiftyoneDegreesMalloc = counting;
		fiftyoneDegreesFree = freeing;
	}
	~BalancedAllocations() {
		fiftyoneDegreesMalloc = original;
		fiftyoneDegreesFree = originalFree;
	}
	static uint64_t calls; // Allocations of at least fromSize
	static int64_t live; // Allocations made and not yet freed
	static uint64_t failAt; // The call to refuse, or zero for none
	static size_t failedSize; // Size of the allocation refused
	static size_t fromSize; // Smaller allocations are not counted
	static vector<size_t>* sizes; // Size of each call counted, if set
private:
	static void* FIFTYONE_DEGREES_CALL_CONV counting(size_t size) {
		if (size >= fromSize) {
			calls++;
			if (sizes != nullptr) {
				sizes->push_back(size);
			}
			if (calls == failAt) {
				failedSize = size;
				return nullptr;
			}
		}
		void* const memory = original(size);
		if (memory != nullptr) {
			live++;
		}
		return memory;
	}
	static void FIFTYONE_DEGREES_CALL_CONV freeing(void* memory) {
		if (memory != nullptr) {
			live--;
		}
		originalFree(memory);
	}
	static void* (FIFTYONE_DEGREES_CALL_CONV *original)(size_t);
	static void (FIFTYONE_DEGREES_CALL_CONV *originalFree)(void*);
};

uint64_t BalancedAllocations::calls = 0;
int64_t BalancedAllocations::live = 0;
uint64_t BalancedAllocations::failAt = 0;
size_t BalancedAllocations::failedSize = 0;
size_t BalancedAllocations::fromSize = 0;
vector<size_t>* BalancedAllocations::sizes = nullptr;
void* (FIFTYONE_DEGREES_CALL_CONV *BalancedAllocations::original)(size_t) =
	nullptr;
void (FIFTYONE_DEGREES_CALL_CONV *BalancedAllocations::originalFree)(void*) =
	nullptr;

class IpiBalancedTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath == "";
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath == "") {
			dataFilePath = GetFilePath(_dataFolderName, balancedAsnFileName);
		}
		if (dataFilePath == "") {
			GTEST_SKIP() << "No data file was found. Run "
				"'git submodule update --init' to fetch one.";
		}
		BalancedManager all(
			dataFilePath,
			fiftyoneDegreesIpiLowMemoryConfig,
			nullptr);
		BalancedDataSet dataSet(&all.manager);
		fiftyoneDegreesPropertiesAvailable* available =
			dataSet->b.b.available;
		vector<bool> seen(256, false);
		int index = fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			available,
			"RegisteredCountry");
		const uint8_t component = available->items[index < 0 ? 0 : index]
			.componentIndex;
		for (uint32_t i = 0; i < available->count; i++) {
			const string name = FIFTYONE_DEGREES_STRING(
				fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
					available,
					(int)i));
			allNames.push_back(name);
			const uint8_t c = available->items[i].componentIndex;
			if (c == component) {
				oneComponent += (oneComponent.empty() ? "" : ",") + name;
			}
			if (seen[c] == false) {
				seen[c] = true;
				oneOfEach += (oneOfEach.empty() ? "" : ",") + name;
				componentCount++;
			}
			if (i + 1 < available->count) {
				allButOne += (allButOne.empty() ? "" : ",") + name;
			}
		}
	}
	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		string().swap(dataFilePath);
		string().swap(oneComponent);
		string().swap(oneOfEach);
		string().swap(allButOne);
		vector<string>().swap(allNames);
		Base::TearDown();
	}
protected:
	string dataFilePath = "";
	string oneComponent = "";
	string oneOfEach = "";
	string allButOne = "";
	vector<string> allNames;
	int componentCount = 0;

	void skipWithOneComponent() {
		if (componentCount < 2) {
			GTEST_SKIP() << "The data file has one component.";
		}
	}

	size_t sizeOf(
		fiftyoneDegreesConfigIpi config,
		const char* required) {
		EXCEPTION_CREATE;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		const size_t size = fiftyoneDegreesIpiSizeManagerFromFile(
			&config,
			&properties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
		return size;
	}

	// The answers from the preset naming the properties match Low Memory.
	void expectSameAnswers(
		const fiftyoneDegreesConfigIpi& preset,
		const char* required) {
		const vector<string> addresses = balancedAddresses();
		BalancedManager expected(
			dataFilePath,
			fiftyoneDegreesIpiLowMemoryConfig,
			required);
		BalancedManager actual(dataFilePath, preset, required);
		EXPECT_EQ(
			balancedAnswers(&expected.manager, addresses),
			balancedAnswers(&actual.manager, addresses));
	}
};

TEST_F(IpiBalancedTests, PresetHoldsPropertyTypesAndProfileOffsetsInMemory) {
	const fiftyoneDegreesConfigIpi* presets[] = {
		&fiftyoneDegreesIpiBalancedConfig,
		&fiftyoneDegreesIpiDefaultConfig,
		&fiftyoneDegreesIpiBalancedTempConfig };
	for (const fiftyoneDegreesConfigIpi* preset : presets) {
		EXPECT_TRUE(preset->propertyTypes.loaded);
		EXPECT_TRUE(preset->profileOffsets.loaded);
		EXPECT_TRUE(preset->strings.loaded);
		EXPECT_TRUE(preset->graph.loaded);
		EXPECT_FALSE(preset->values.loaded);
		EXPECT_FALSE(preset->profiles.loaded);
	}
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		nullptr);
	BalancedDataSet dataSet(&manager.manager);
	EXPECT_STREQ(balancedMemoryType, dataSet->propertyTypes->typeName);
	EXPECT_STREQ(balancedMemoryType, dataSet->profileOffsets->typeName);
}

TEST_F(IpiBalancedTests, NamingNothingKeepsThePreset) {
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		nullptr);
	{
		BalancedDataSet dataSet(&manager.manager);
		EXPECT_STREQ(balancedMemoryType, dataSet->strings->typeName);
		EXPECT_STREQ(balancedCacheType, dataSet->values->typeName);
		EXPECT_STREQ(balancedCacheType, dataSet->profiles->typeName);
		EXPECT_EQ(
			fiftyoneDegreesIpiBalancedConfig.profiles.capacity,
			balancedCacheCapacity(dataSet->profiles));
		for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
			EXPECT_STREQ(
				balancedMemoryType,
				dataSet->graphsArray->items[g].nodes->typeName);
		}
	}
	EXPECT_NO_FATAL_FAILURE(expectSameAnswers(
		fiftyoneDegreesIpiBalancedConfig,
		nullptr));
}

TEST_F(IpiBalancedTests, NamingEveryPropertyKeepsThePreset) {
	vector<const char*> names;
	for (const string& name : allNames) {
		names.push_back(name.c_str());
	}
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.array = names.data();
	properties.count = (int)names.size();
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		properties);
	EXPECT_STREQ(
		balancedMemoryType,
		BalancedDataSet(&manager.manager)->strings->typeName);
}

TEST_F(IpiBalancedTests, OtherPresetsAreUnchangedByNaming) {
	const string memory = string(balancedMemoryType) + " " + balancedMemoryType +
		" " + balancedMemoryType + " " + balancedMemoryType + " " +
		balancedMemoryType;
	const string file = string(balancedFileType) + " " + balancedFileType + " " +
		balancedFileType + " " + balancedFileType + " " + balancedFileType;
	BalancedManager high(
		dataFilePath,
		fiftyoneDegreesIpiHighPerformanceConfig,
		oneComponent.c_str());
	EXPECT_EQ(memory, balancedTypes(&high.manager));
	BalancedManager low(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	EXPECT_EQ(file, balancedTypes(&low.manager));
	const string smallFile = GetFilePath(_dataFolderName, balancedAsnFileName);
	if (smallFile.empty() == false) {
		BalancedManager inMemory(
			smallFile,
			fiftyoneDegreesIpiInMemoryConfig,
			"Asn");
		EXPECT_EQ(memory, balancedTypes(&inMemory.manager));
	}
}

TEST_F(IpiBalancedTests, NamedPropertiesGiveTheSameAnswers) {
	EXPECT_NO_FATAL_FAILURE(expectSameAnswers(
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str()));
	EXPECT_NO_FATAL_FAILURE(expectSameAnswers(
		fiftyoneDegreesIpiBalancedConfig,
		oneOfEach.c_str()));
	EXPECT_NO_FATAL_FAILURE(expectSameAnswers(
		fiftyoneDegreesIpiBalancedConfig,
		allButOne.c_str()));
}

TEST_F(IpiBalancedTests, NamingNeverUsesMoreMemory) {
	const size_t nothing = sizeOf(fiftyoneDegreesIpiBalancedConfig, nullptr);
	EXPECT_LE(
		sizeOf(fiftyoneDegreesIpiBalancedConfig, oneComponent.c_str()),
		nothing);
	EXPECT_LE(
		sizeOf(fiftyoneDegreesIpiBalancedConfig, oneOfEach.c_str()),
		nothing);
	EXPECT_LE(
		sizeOf(fiftyoneDegreesIpiBalancedConfig, allButOne.c_str()),
		nothing);
}

TEST_F(IpiBalancedTests, GraphsOfOtherComponentsStayOnFile) {
	skipWithOneComponent();
	if (IsSkipped()) {
		return;
	}
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	BalancedDataSet dataSet(&manager.manager);
	vector<bool> required(256, false);
	for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
		if (dataSet->componentsAvailable[c]) {
			required[((const fiftyoneDegreesComponent*)dataSet->componentsList
				.items[c].data.ptr)->componentId] = true;
		}
	}
	for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
		const fiftyoneDegreesIpiCg& graph = dataSet->graphsArray->items[g];
		const char* expected = required[graph.info.componentId] ?
			balancedMemoryType : balancedFileType;
		EXPECT_STREQ(expected, graph.nodes->typeName);
		EXPECT_STREQ(expected, graph.spans->typeName);
		EXPECT_STREQ(expected, graph.spanBytes->typeName);
		EXPECT_STREQ(expected, graph.clusters->typeName);
	}
}

TEST_F(IpiBalancedTests, EveryGraphCanStillBeEvaluated) {
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	BalancedDataSet e(&expected.manager);
	BalancedDataSet a(&actual.manager);
	for (const string& address : balancedAddresses()) {
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
}

TEST_F(IpiBalancedTests, EveryProfileCanStillBeRead) {
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	BalancedDataSet e(&expected.manager);
	BalancedDataSet a(&actual.manager);
	const uint32_t count = fiftyoneDegreesCollectionGetCount(a->profileOffsets);
	const uint32_t step = count > 2000 ? count / 2000 : 1;
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
		FIFTYONE_DEGREES_COLLECTION_RELEASE(e->profiles, &expectedItem);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(a->profiles, &actualItem);
	}
}

TEST_F(IpiBalancedTests, EveryValueAndStringCanStillBeRead) {
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	BalancedDataSet e(&expected.manager);
	BalancedDataSet a(&actual.manager);
	const uint32_t count = fiftyoneDegreesCollectionGetCount(a->values);
	const uint32_t step = count > 2000 ? count / 2000 : 1;
	for (uint32_t i = 0; i < count; i += step) {
		EXCEPTION_CREATE;
		fiftyoneDegreesCollectionItem expectedItem, actualItem;
		fiftyoneDegreesDataReset(&expectedItem.data);
		fiftyoneDegreesDataReset(&actualItem.data);
		const fiftyoneDegreesValue* expectedValue = fiftyoneDegreesValueGet(
			e->values, i, &expectedItem, exception);
		EXCEPTION_THROW;
		const fiftyoneDegreesValue* actualValue = fiftyoneDegreesValueGet(
			a->values, i, &actualItem, exception);
		EXCEPTION_THROW;
		ASSERT_NE(nullptr, expectedValue);
		ASSERT_NE(nullptr, actualValue);
		ASSERT_EQ(
			0,
			memcmp(expectedValue, actualValue, sizeof(fiftyoneDegreesValue))) <<
			"Value index " << i;
		const fiftyoneDegreesPropertyValueType type =
			fiftyoneDegreesPropertyGetStoredTypeByIndex(
				a->propertyTypes,
				(uint32_t)actualValue->propertyIndex,
				exception);
		EXCEPTION_THROW;
		const uint32_t offset = (uint32_t)actualValue->nameOffset;
		FIFTYONE_DEGREES_COLLECTION_RELEASE(e->values, &expectedItem);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(a->values, &actualItem);

		fiftyoneDegreesDataReset(&expectedItem.data);
		fiftyoneDegreesDataReset(&actualItem.data);
		const void* expectedText = fiftyoneDegreesStoredBinaryValueGet(
			e->strings, offset, type, &expectedItem, exception);
		EXCEPTION_THROW;
		const void* actualText = fiftyoneDegreesStoredBinaryValueGet(
			a->strings, offset, type, &actualItem, exception);
		EXCEPTION_THROW;
		ASSERT_NE(nullptr, expectedText);
		ASSERT_NE(nullptr, actualText);
		const fiftyoneDegreesCollectionKeyType* keyType =
			fiftyoneDegreesGetCollectionKeyTypeForStoredValueType(
				type,
				exception);
		EXCEPTION_THROW;
		const uint32_t size = keyType->getFinalSizeMethod != nullptr ?
			keyType->getFinalSizeMethod(expectedText, exception) :
			keyType->initialBytesCount;
		EXCEPTION_THROW;
		EXPECT_EQ(0, memcmp(expectedText, actualText, size)) <<
			"String of value index " << i;
		FIFTYONE_DEGREES_COLLECTION_RELEASE(e->strings, &expectedItem);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(a->strings, &actualItem);
	}
}

TEST_F(IpiBalancedTests, DefaultValuesReadForALookupOfNothing) {
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	vector<char> buffer(balancedBufferBytes);
	fiftyoneDegreesResultsIpi* expectedResults =
		fiftyoneDegreesResultsIpiCreate(&expected.manager);
	fiftyoneDegreesResultsIpi* actualResults =
		fiftyoneDegreesResultsIpiCreate(&actual.manager);
	const int count = (int)((const fiftyoneDegreesDataSetIpi*)
		actualResults->b.dataSet)->b.b.available->count;
	const int none = 0;
	const string address = balancedAddresses()[0];
	EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
		expectedResults, address.c_str(), address.size(), &none, 0, exception);
	EXCEPTION_THROW;
	fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
		actualResults, address.c_str(), address.size(), &none, 0, exception);
	EXCEPTION_THROW;
	for (int i = 0; i < count; i++) {
		EXPECT_EQ(
			balancedAnswer(expectedResults, i, buffer),
			balancedAnswer(actualResults, i, buffer)) << "Required index " << i;
	}
	fiftyoneDegreesResultsIpiFree(expectedResults);
	fiftyoneDegreesResultsIpiFree(actualResults);
}

TEST_F(IpiBalancedTests, PropertyThatWasNotNamedGivesWhatItGaveBefore) {
	skipWithOneComponent();
	if (IsSkipped()) {
		return;
	}
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	vector<char> buffer(balancedBufferBytes);
	fiftyoneDegreesResultsIpi* expectedResults =
		fiftyoneDegreesResultsIpiCreate(&expected.manager);
	fiftyoneDegreesResultsIpi* actualResults =
		fiftyoneDegreesResultsIpiCreate(&actual.manager);
	const string address = balancedAddresses()[0];
	balancedDetect(expectedResults, address);
	balancedDetect(actualResults, address);
	for (const string& name : allNames) {
		EXCEPTION_CREATE;
		const size_t expectedAdded = fiftyoneDegreesResultsIpiGetValuesString(
			expectedResults,
			name.c_str(),
			buffer.data(),
			buffer.size(),
			"|",
			exception);
		const bool expectedFailed = EXCEPTION_FAILED;
		const string expectedText = expectedAdded > 0 ? buffer.data() : "";
		EXCEPTION_CLEAR;
		const size_t actualAdded = fiftyoneDegreesResultsIpiGetValuesString(
			actualResults,
			name.c_str(),
			buffer.data(),
			buffer.size(),
			"|",
			exception);
		EXPECT_EQ(expectedFailed, EXCEPTION_FAILED) << name;
		EXPECT_EQ(expectedAdded, actualAdded) << name;
		EXPECT_EQ(expectedText, actualAdded > 0 ? buffer.data() : "") << name;
	}
	fiftyoneDegreesResultsIpiFree(expectedResults);
	fiftyoneDegreesResultsIpiFree(actualResults);
}

TEST_F(IpiBalancedTests, ReloadHoldsTheSame) {
	BalancedManager expected(
		dataFilePath,
		fiftyoneDegreesIpiLowMemoryConfig,
		oneComponent.c_str());
	BalancedManager actual(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	const string before = balancedTypes(&actual.manager);
	EXCEPTION_CREATE;
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesIpiReloadManagerFromOriginalFile(
			&actual.manager,
			exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
	EXCEPTION_THROW;
	EXPECT_EQ(before, balancedTypes(&actual.manager));
	const vector<string> addresses = balancedAddresses();
	EXPECT_EQ(
		balancedAnswers(&expected.manager, addresses),
		balancedAnswers(&actual.manager, addresses));
}

TEST_F(IpiBalancedTests, TemporaryFilePresetHoldsTheSame) {
	const string smallFile = GetFilePath(_dataFolderName, balancedAsnFileName);
	if (smallFile.empty()) {
		GTEST_SKIP() << "The small data file was not found.";
	}
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedTempConfig;
	config.b.reuseTempFile = false;
	BalancedManager expected(
		smallFile,
		fiftyoneDegreesIpiLowMemoryConfig,
		"Asn");
	BalancedManager actual(smallFile, config, "Asn");
	EXPECT_STREQ(
		balancedHeldType,
		BalancedDataSet(&actual.manager)->strings->typeName);
	const vector<string> addresses = balancedAddresses();
	EXPECT_EQ(
		balancedAnswers(&expected.manager, addresses),
		balancedAnswers(&actual.manager, addresses));
}

TEST_F(IpiBalancedTests, NamesThatMatchNothingFailAsBefore) {
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = "NotAProperty";
	EXCEPTION_CREATE;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		fiftyoneDegreesIpiInitManagerFromFile(
			&manager,
			&config,
			&properties,
			dataFilePath.c_str(),
			exception));
}

TEST_F(IpiBalancedTests, EngineMetaDataMatches) {
	Common::RequiredPropertiesConfig properties(oneComponent);
	Ipi::ConfigIpi expectedConfig;
	expectedConfig.setLowMemory();
	Ipi::ConfigIpi actualConfig;
	actualConfig.setBalanced();
	Ipi::EngineIpi expected(dataFilePath, &expectedConfig, &properties);
	Ipi::EngineIpi actual(dataFilePath, &actualConfig, &properties);
	using Components = Common::Collection<::byte, Common::ComponentMetaData>;
	using Profiles = Common::Collection<uint32_t, Common::ProfileMetaData>;
	using Properties = Common::Collection<string, Common::PropertyMetaData>;

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

	unique_ptr<Properties> expectedNames(
		expected.getMetaData()->getProperties());
	unique_ptr<Properties> actualNames(actual.getMetaData()->getProperties());
	ASSERT_EQ(expectedNames->getSize(), actualNames->getSize());
	for (uint32_t i = 0; i < actualNames->getSize(); i++) {
		unique_ptr<Common::PropertyMetaData> e(expectedNames->getByIndex(i));
		unique_ptr<Common::PropertyMetaData> a(actualNames->getByIndex(i));
		EXPECT_EQ(e->getName(), a->getName());
		EXPECT_EQ(e->getDefaultValue(), a->getDefaultValue());
		EXPECT_EQ(e->getComponentId(), a->getComponentId());
	}
}

/**
 * Compares the answers of each thread with those from one thread.
 */
class IpiBalancedThreadTests : public IpiBalancedTests {
public:
	static void runThread(void* state) {
		IpiBalancedThreadTests* const test = (IpiBalancedThreadTests*)state;
		if (balancedAnswers(test->threadManager, test->threadAddresses) !=
			test->threadExpected) {
			test->threadFailures++;
		}
		FIFTYONE_DEGREES_THREAD_EXIT;
	}
	void TearDown() override {
		vector<string>().swap(threadAddresses);
		vector<string>().swap(threadExpected);
		IpiBalancedTests::TearDown();
	}
protected:
	fiftyoneDegreesResourceManager* threadManager = nullptr;
	vector<string> threadAddresses;
	vector<string> threadExpected;
	atomic<int> threadFailures{ 0 };
};

TEST_F(IpiBalancedThreadTests, ThreadsGetTheSameAnswers) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		GTEST_SKIP() << "Built without threading.";
	}
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneOfEach.c_str());
	threadManager = &manager.manager;
	threadAddresses = balancedAddresses();
	threadExpected = balancedAnswers(threadManager, threadAddresses);
	runThreads(
		4,
		(FIFTYONE_DEGREES_THREAD_ROUTINE)IpiBalancedThreadTests::runThread);
	EXPECT_EQ(0, threadFailures.load());
}

/**
 * Tests that need a data file with particular contents, written to a file only
 * the test uses from the small data file, or that count the allocations of the
 * data set.
 */
class IpiBalancedFileTests : public IpiBalancedTests {
public:
	void TearDown() override {
		if (tempFilePath.empty() == false) {
			EXPECT_EQ(0, remove(tempFilePath.c_str())) <<
				"Could not delete '" << tempFilePath << "'.";
		}
		std::vector<unsigned char>().swap(data);
		string().swap(tempFilePath);
		IpiBalancedTests::TearDown();
	}
protected:
	std::vector<unsigned char> data;
	string tempFilePath;

	void readSmallFile() {
		const string path = GetFilePath(_dataFolderName, balancedAsnFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The small data file was not found.";
		}
		FILE* file = fopen(path.c_str(), "rb");
		ASSERT_NE(nullptr, file);
		fseek(file, 0, SEEK_END);
		data.resize((size_t)ftell(file));
		rewind(file);
		const size_t read = fread(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), read);
	}

	void writeTempFile() {
#ifdef _WIN32
		char directory[MAX_PATH];
		char path[MAX_PATH];
		ASSERT_NE(0u, GetTempPathA(MAX_PATH, directory));
		ASSERT_NE(0u, GetTempFileNameA(directory, "ipi", 0, path));
#else
		char path[] = "/tmp/ipi_balanced_test_XXXXXX";
		const int descriptor = mkstemp(path);
		ASSERT_NE(-1, descriptor);
		close(descriptor);
#endif
		tempFilePath = path;
		FILE* file = fopen(tempFilePath.c_str(), "wb");
		ASSERT_NE(nullptr, file);
		const size_t written = fwrite(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), written);
	}

	const fiftyoneDegreesDataSetIpiHeader* header() const {
		return (const fiftyoneDegreesDataSetIpiHeader*)data.data();
	}

	/**
	 * Refuses each allocation in turn from the one given, checking that the
	 * manager fails and that everything it allocated was freed.
	 */
	void expectEachFailureFreesEverything(
		const string& path,
		const char* required,
		const uint64_t firstCall,
		const size_t fromSize) {
		BalancedAllocations allocations;
		BalancedAllocations::fromSize = fromSize;
		for (uint64_t call = firstCall; call > 0; call++) {
			fiftyoneDegreesResourceManager manager;
			fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
			fiftyoneDegreesPropertiesRequired properties =
				fiftyoneDegreesPropertiesDefault;
			properties.string = required;
			BalancedAllocations::calls = 0;
			BalancedAllocations::live = 0;
			BalancedAllocations::failAt = call;
			EXCEPTION_CREATE;
			const fiftyoneDegreesStatusCode status =
				fiftyoneDegreesIpiInitManagerFromFile(
					&manager,
					&config,
					&properties,
					path.c_str(),
					exception);
			const bool failed = status != FIFTYONE_DEGREES_STATUS_SUCCESS ||
				EXCEPTION_FAILED;
			if (BalancedAllocations::calls < call) {
				// Every allocation has been refused in turn.
				ASSERT_FALSE(failed);
				fiftyoneDegreesResourceManagerFree(&manager);
				break;
			}
			if (failed == false) {
				// The code carries on when it can do without the memory.
				fiftyoneDegreesResourceManagerFree(&manager);
			}
			EXPECT_EQ(0, BalancedAllocations::live) <<
				"Leak when call " << call << " of " <<
				BalancedAllocations::failedSize << " bytes was refused.";
		}
	}
};

TEST_F(IpiBalancedFileTests, DefaultValueOutsideItsPropertyIsHeld) {
	ASSERT_NO_FATAL_FAILURE(readSmallFile());
	if (IsSkipped()) {
		return;
	}
	// Point the default value of the first property at a value of the second,
	// and make the property mandatory so that the default is used.
	const uint64_t properties = header()->properties.startPosition;
	const fiftyoneDegreesProperty* first =
		(const fiftyoneDegreesProperty*)(data.data() + properties);
	const uint32_t outside = (first + 1)->firstValueIndex;
	ASSERT_GT(outside, (uint32_t)first->lastValueIndex);
	memcpy(
		data.data() + properties +
			offsetof(fiftyoneDegreesProperty, defaultValueIndex),
		&outside,
		sizeof(outside));
	data[(size_t)properties + offsetof(fiftyoneDegreesProperty, isMandatory)] = 1;
	ASSERT_NO_FATAL_FAILURE(writeTempFile());

	{
		BalancedManager expected(
			tempFilePath,
			fiftyoneDegreesIpiLowMemoryConfig,
			"Asn");
		BalancedManager actual(
			tempFilePath,
			fiftyoneDegreesIpiBalancedConfig,
			"Asn");
		{
			BalancedDataSet dataSet(&actual.manager);
			ASSERT_STREQ(balancedHeldType, dataSet->values->typeName);
			EXCEPTION_CREATE;
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			ASSERT_NE(nullptr, fiftyoneDegreesValueGet(
				dataSet->values,
				outside,
				&item,
				exception));
			EXCEPTION_THROW;
			EXPECT_EQ(nullptr, item.handle) <<
				"The default value was read from the file.";
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &item);
		}
		vector<char> buffer(balancedBufferBytes);
		fiftyoneDegreesResultsIpi* expectedResults =
			fiftyoneDegreesResultsIpiCreate(&expected.manager);
		fiftyoneDegreesResultsIpi* actualResults =
			fiftyoneDegreesResultsIpiCreate(&actual.manager);
		const int none = 0;
		const string address = balancedAddresses()[0];
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
			expectedResults,
			address.c_str(),
			address.size(),
			&none,
			0,
			exception);
		EXCEPTION_THROW;
		fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
			actualResults,
			address.c_str(),
			address.size(),
			&none,
			0,
			exception);
		EXCEPTION_THROW;
		const string answer = balancedAnswer(actualResults, 0, buffer);
		EXPECT_EQ(
			"1", answer.substr(0, 1)) << "The default value was not found.";
		EXPECT_EQ(balancedAnswer(expectedResults, 0, buffer), answer);
		fiftyoneDegreesResultsIpiFree(expectedResults);
		fiftyoneDegreesResultsIpiFree(actualResults);
	}
}

TEST_F(IpiBalancedFileTests, NothingIsAllocatedDuringALookupOfHeldData) {
	skipWithOneComponent();
	if (IsSkipped()) {
		return;
	}
	BalancedManager manager(
		dataFilePath,
		fiftyoneDegreesIpiBalancedConfig,
		oneComponent.c_str());
	{
		BalancedDataSet dataSet(&manager.manager);
		if (string(dataSet->values->typeName) != balancedHeldType ||
			string(dataSet->profiles->typeName) != balancedHeldType) {
			GTEST_SKIP() << "The data file does not leave enough on file.";
		}
	}
	const vector<string> addresses = balancedAddresses();
	vector<char> buffer(balancedBufferBytes);
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&manager.manager);
	const int count = (int)((const fiftyoneDegreesDataSetIpi*)
		results->b.dataSet)->b.b.available->count;
	// A first pass gives each list in the results the size it needs.
	const auto pass = [&]() {
		for (const string& address : addresses) {
			balancedDetect(results, address);
			for (int i = 0; i < count; i++) {
				balancedAnswer(results, i, buffer);
			}
		}
	};
	pass();
	{
		BalancedAllocations allocations;
		pass();
		EXPECT_EQ(0u, BalancedAllocations::calls);
	}
	fiftyoneDegreesResultsIpiFree(results);
}

TEST_F(IpiBalancedFileTests, TruncatedFileFailsWhenStringsAreHeld) {
	ASSERT_NO_FATAL_FAILURE(readSmallFile());
	if (IsSkipped()) {
		return;
	}
	// Cut the file part way through the strings. Holding them would otherwise
	// be the first thing to read them.
	data.resize((size_t)header()->strings.startPosition + 1024);
	ASSERT_NO_FATAL_FAILURE(writeTempFile());
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = "Asn";
	EXCEPTION_CREATE;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		fiftyoneDegreesIpiInitManagerFromFile(
			&manager,
			&config,
			&properties,
			tempFilePath.c_str(),
			exception));
}

#ifdef NDEBUG
// The cache asserts that its allocation worked, so these only run with asserts
// compiled out.
TEST_F(IpiBalancedFileTests, FailedLargeAllocationFreesEverything) {
	expectEachFailureFreesEverything(
		dataFilePath,
		oneComponent.c_str(),
		1,
		1024 * 1024);
}

TEST_F(IpiBalancedFileTests, FailedAllocationAfterTheScanBeginsFreesEverything) {
	// The scan buffer is the first allocation of its size.
	const size_t scanBufferBytes = 65536;
	vector<size_t> sizes;
	{
		BalancedAllocations allocations;
		BalancedAllocations::sizes = &sizes;
		{
			BalancedManager manager(
				dataFilePath,
				fiftyoneDegreesIpiBalancedConfig,
				oneComponent.c_str());
		}
		BalancedAllocations::sizes = nullptr;
	}
	uint64_t scanBufferCall = 0;
	for (size_t i = 0; i < sizes.size() && scanBufferCall == 0; i++) {
		if (sizes[i] == scanBufferBytes) {
			scanBufferCall = (uint64_t)i + 1;
		}
	}
	vector<size_t>().swap(sizes);
	ASSERT_GT(scanBufferCall, 0u);
	expectEachFailureFreesEverything(
		dataFilePath,
		oneComponent.c_str(),
		scanBufferCall,
		0);
}

TEST_F(IpiBalancedFileTests, FailedAllocationOfTheLargerCacheFreesEverything) {
	// One property of a large component leaves its profiles on file, which
	// enlarges the profile cache.
	const string required = allNames.front();
	expectEachFailureFreesEverything(
		dataFilePath,
		required.c_str(),
		1,
		64 * 1024 + 1);
}
#endif
