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
 * @file HashPresetTests.cpp
 * @brief Tests that loadRequiredPropertiesOnly, and not the shape of a
 * preset, chooses whether only the values and strings the required properties
 * read are held in memory.
 *
 * Every answer is compared with the same configuration with the setting off.
 * The tests use the Lite and TAC data files, whichever are found.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/common/memory.h"
#include "../../../src/dd/hash/ConfigHash.hpp"
#include "../../../src/dd/hash/hash.h"

namespace {

const char* const heldType = "CollectionHeld";
const char* const memoryType = "CollectionMemory";

/** The data files these tests use. */
const char* const presetFileNames[] = {
	"TAC-HashV41.hash",
	"51Degrees-LiteV4.1.hash" };

const char* const presetUserAgents[] = {
	"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
	"AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 "
	"Safari/604.1",
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
	"Mozilla/5.0 (Linux; Android 14; SM-S918B) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36",
	"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
	"(KHTML, like Gecko) Version/17.1 Safari/605.1.15",
	"Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0",
	"Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
	"Mozilla/5.0 (Linux; Android 9; TAC-0000) AppleWebKit/537.36",
	"not a user agent" };

/**
 * The state of a held collection, laid out as collectionHeld in
 * src/common/collection.c, which the tests read to report and check what is
 * held.
 */
struct HeldRange {
	uint32_t first;
	uint32_t end;
	uint8_t* memory;
};
struct HeldState {
	fiftyoneDegreesCollection* source;
	uint64_t bytes;
	uint32_t unitBytes;
	uint32_t count;
	HeldRange* ranges;
};

/**
 * The bytes a collection has in memory, which is none for one that reads the
 * file or a cache.
 */
uint64_t bytesInMemory(const fiftyoneDegreesCollection* collection) {
	const std::string type = collection->typeName;
	if (type == memoryType) {
		const fiftyoneDegreesCollectionMemory* memory =
			(const fiftyoneDegreesCollectionMemory*)collection->state;
		return (uint64_t)(memory->lastByte - memory->firstByte);
	}
	if (type == heldType) {
		return ((const HeldState*)collection->state)->bytes;
	}
	return 0;
}

/**
 * A manager over a data file with the required properties given, or every
 * property when there are none, which is freed however the test ends.
 */
class PresetManager {
public:
	PresetManager(
		const std::string& path,
		fiftyoneDegreesConfigHash config,
		const char* required) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		init(path, config, properties);
	}
	PresetManager(
		const std::string& path,
		fiftyoneDegreesConfigHash config,
		fiftyoneDegreesPropertiesRequired properties) {
		init(path, config, properties);
	}
	~PresetManager() {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
	fiftyoneDegreesResourceManager manager;
private:
	void init(
		const std::string& path,
		fiftyoneDegreesConfigHash& config,
		fiftyoneDegreesPropertiesRequired& properties) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path.c_str(),
				exception);
		if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
			throw FiftyoneDegrees::Common::StatusCodeException(status);
		}
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}
};

/** The data set of a manager for the length of a scope. */
class PresetDataSet {
public:
	explicit PresetDataSet(fiftyoneDegreesResourceManager* manager) {
		dataSet = fiftyoneDegreesDataSetHashGet(manager);
	}
	~PresetDataSet() {
		fiftyoneDegreesDataSetHashRelease(dataSet);
	}
	fiftyoneDegreesDataSetHash* operator->() const {
		return dataSet;
	}
	fiftyoneDegreesDataSetHash* dataSet;
};

/** The values of every available property for each user agent. */
std::vector<std::string> presetAnswers(
	fiftyoneDegreesResourceManager* manager) {
	std::vector<std::string> answers;
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(manager, 1, 0);
	std::vector<char> buffer(1024 * 64);
	char separator[] = "|";
	for (const char* userAgent : presetUserAgents) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			userAgent,
			strlen(userAgent),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		FIFTYONE_DEGREES_EXCEPTION_CLEAR;
		buffer[0] = '\0';
		fiftyoneDegreesResultsHashGetValuesStringAllProperties(
			results,
			buffer.data(),
			buffer.size(),
			separator,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		answers.push_back(buffer.data());
	}
	fiftyoneDegreesResultsHashFree(results);
	return answers;
}

}

class HashPresetFlagTests : public Base {};

TEST_F(HashPresetFlagTests, PresetsSetTheFlag) {
	EXPECT_FALSE(fiftyoneDegreesHashInMemoryConfig.loadRequiredPropertiesOnly);
	EXPECT_TRUE(
		fiftyoneDegreesHashHighPerformanceConfig.loadRequiredPropertiesOnly);
	EXPECT_FALSE(fiftyoneDegreesHashLowMemoryConfig.loadRequiredPropertiesOnly);
	EXPECT_FALSE(fiftyoneDegreesHashBalancedConfig.loadRequiredPropertiesOnly);
	EXPECT_FALSE(
		fiftyoneDegreesHashBalancedTempConfig.loadRequiredPropertiesOnly);
	EXPECT_FALSE(fiftyoneDegreesHashDefaultConfig.loadRequiredPropertiesOnly);
}

TEST_F(HashPresetFlagTests, EachProfileSetsTheFlagInCpp) {
	using namespace FiftyoneDegrees::DeviceDetection::Hash;
	EXPECT_FALSE(ConfigHash().getLoadRequiredPropertiesOnly());
	EXPECT_FALSE(ConfigHash(nullptr).getLoadRequiredPropertiesOnly());
	ConfigHash config;
	config.setHighPerformance();
	EXPECT_TRUE(config.getLoadRequiredPropertiesOnly());
	config.setLowMemory();
	EXPECT_FALSE(config.getLoadRequiredPropertiesOnly());
	config.setHighPerformance();
	config.setBalanced();
	EXPECT_FALSE(config.getLoadRequiredPropertiesOnly());
	config.setHighPerformance();
	config.setBalancedTemp();
	EXPECT_FALSE(config.getLoadRequiredPropertiesOnly());
	config.setHighPerformance();
	config.setMaxPerformance();
	EXPECT_FALSE(config.getLoadRequiredPropertiesOnly());
}

TEST_F(HashPresetFlagTests, SettersReachTheConfigurationInCpp) {
	using namespace FiftyoneDegrees::DeviceDetection::Hash;
	ConfigHash config;
	config.setLoadRequiredPropertiesOnly(true);
	EXPECT_TRUE(config.getConfig()->loadRequiredPropertiesOnly);
	EXPECT_TRUE(config.getLoadRequiredPropertiesOnly());
	config.setLoadRequiredPropertiesOnly(false);
	EXPECT_FALSE(config.getConfig()->loadRequiredPropertiesOnly);
	EXPECT_FALSE(config.getLoadRequiredPropertiesOnly());
}

/**
 * Runs each test against the data files found. A file with only one property
 * cannot name some but not all of them, so it is skipped.
 */
class HashPresetTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (const char* name : presetFileNames) {
			const std::string path = GetFilePath(_dataFolderName, name);
			if (path.empty() == false) {
				paths.push_back(path);
			}
		}
		ASSERT_FALSE(paths.empty()) << "No data file was found.";
	}
	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<std::string>().swap(paths);
		std::vector<std::string>().swap(allNames);
		std::string().swap(someNames);
		Base::TearDown();
	}
protected:
	std::vector<std::string> paths;
	std::vector<std::string> allNames;
	std::string someNames;

	/**
	 * Finds the names of every property of the file, and some but not all of
	 * them, as a list. Returns false if the file has too few properties.
	 */
	bool findNames(const std::string& path) {
		allNames.clear();
		PresetManager manager(
			path,
			fiftyoneDegreesHashHighPerformanceConfig,
			(const char*)nullptr);
		PresetDataSet dataSet(&manager.manager);
		const fiftyoneDegreesPropertiesAvailable* available =
			dataSet->b.b.available;
		for (uint32_t i = 0; i < available->count; i++) {
			const fiftyoneDegreesString* name =
				(const fiftyoneDegreesString*)available->items[i].name.data.ptr;
			allNames.push_back(&name->value);
		}
		if (allNames.size() < 2) {
			return false;
		}
		const size_t some = allNames.size() > 2 ? 2 : 1;
		someNames.clear();
		for (size_t i = 0; i < some; i++) {
			someNames += (i > 0 ? "," : "") + allNames[i];
		}
		return true;
	}

	static fiftyoneDegreesConfigHash withFlag(
		fiftyoneDegreesConfigHash config,
		bool load) {
		config.loadRequiredPropertiesOnly = load;
		return config;
	}

	static void expectHeld(fiftyoneDegreesResourceManager* manager) {
		PresetDataSet dataSet(manager);
		EXPECT_STREQ(heldType, dataSet->strings->typeName);
		EXPECT_STREQ(heldType, dataSet->values->typeName);
	}

	void expectSameAnswers(
		const std::string& path,
		fiftyoneDegreesConfigHash config,
		const char* required) {
		PresetManager off(path, withFlag(config, false), required);
		PresetManager on(path, withFlag(config, true), required);
		EXPECT_EQ(presetAnswers(&off.manager), presetAnswers(&on.manager))
			<< path << " naming " << (required == nullptr ? "none" : required);
	}
};

TEST_F(HashPresetTests, HighPerformanceNamingSomeHoldsOnlyWhatTheyRead) {
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		PresetManager off(
			path,
			withFlag(fiftyoneDegreesHashHighPerformanceConfig, false),
			someNames.c_str());
		PresetManager on(
			path,
			fiftyoneDegreesHashHighPerformanceConfig,
			someNames.c_str());
		EXPECT_EQ(presetAnswers(&off.manager), presetAnswers(&on.manager))
			<< path;
		{
			PresetDataSet dataSet(&off.manager);
			EXPECT_STREQ(memoryType, dataSet->strings->typeName) << path;
			EXPECT_STREQ(memoryType, dataSet->values->typeName) << path;
			PresetDataSet held(&on.manager);
			ASSERT_STREQ(heldType, held->strings->typeName) << path;
			ASSERT_STREQ(heldType, held->values->typeName) << path;
			printf("%s naming %s holds %llu of %llu bytes of values and "
				"%llu of %llu bytes of strings\n",
				path.c_str(),
				someNames.c_str(),
				(unsigned long long)bytesInMemory(held->values),
				(unsigned long long)bytesInMemory(dataSet->values),
				(unsigned long long)bytesInMemory(held->strings),
				(unsigned long long)bytesInMemory(dataSet->strings));
			EXPECT_LT(
				bytesInMemory(held->values),
				bytesInMemory(dataSet->values)) << path;
			EXPECT_LT(
				bytesInMemory(held->strings),
				bytesInMemory(dataSet->strings)) << path;
			// The profiles and nodes every lookup walks follow their own
			// settings, which load them into memory in this preset.
			EXPECT_STREQ(memoryType, held->profiles->typeName) << path;
			EXPECT_STREQ(memoryType, held->nodes->typeName) << path;
		}
	}
}

TEST_F(HashPresetTests, HighPerformanceWithoutTheFlagLoadsEverything) {
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		PresetManager manager(
			path,
			withFlag(fiftyoneDegreesHashHighPerformanceConfig, false),
			someNames.c_str());
		PresetDataSet dataSet(&manager.manager);
		EXPECT_STREQ(memoryType, dataSet->strings->typeName) << path;
		EXPECT_STREQ(memoryType, dataSet->values->typeName) << path;
	}
}

TEST_F(HashPresetTests, HighPerformanceChangesNothingWhenNoneOrEveryIsNamed) {
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		std::vector<const char*> names;
		for (const std::string& name : allNames) {
			names.push_back(name.c_str());
		}
		fiftyoneDegreesPropertiesRequired every =
			fiftyoneDegreesPropertiesDefault;
		every.array = names.data();
		every.count = (int)names.size();
		const fiftyoneDegreesPropertiesRequired namings[] = {
			fiftyoneDegreesPropertiesDefault,
			every };
		for (const fiftyoneDegreesPropertiesRequired& properties : namings) {
			PresetManager off(
				path,
				withFlag(fiftyoneDegreesHashHighPerformanceConfig, false),
				properties);
			PresetManager on(
				path,
				fiftyoneDegreesHashHighPerformanceConfig,
				properties);
			PresetDataSet a(&off.manager);
			PresetDataSet b(&on.manager);
			EXPECT_STREQ(memoryType, b->strings->typeName) << path;
			EXPECT_STREQ(memoryType, b->values->typeName) << path;
			EXPECT_EQ(
				bytesInMemory(a->strings),
				bytesInMemory(b->strings)) << path;
			EXPECT_EQ(
				bytesInMemory(a->values),
				bytesInMemory(b->values)) << path;
			EXPECT_EQ(presetAnswers(&off.manager), presetAnswers(&on.manager))
				<< path;
		}
	}
}

TEST_F(HashPresetTests, ACustomConfigurationHoldsOnlyWhenTheFlagIsSet) {
	const fiftyoneDegreesConfigHash* shapes[] = {
		&fiftyoneDegreesHashLowMemoryConfig,
		&fiftyoneDegreesHashBalancedConfig };
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		for (const fiftyoneDegreesConfigHash* shape : shapes) {
			PresetManager off(
				path, withFlag(*shape, false), someNames.c_str());
			PresetManager on(path, withFlag(*shape, true), someNames.c_str());
			{
				PresetDataSet dataSet(&off.manager);
				EXPECT_STRNE(heldType, dataSet->strings->typeName) << path;
				EXPECT_STRNE(heldType, dataSet->values->typeName) << path;
			}
			EXPECT_NO_FATAL_FAILURE(expectHeld(&on.manager)) << path;
			EXPECT_EQ(presetAnswers(&off.manager), presetAnswers(&on.manager))
				<< path;
		}
	}
}

TEST_F(HashPresetTests, TheFlagChangesNothingWhenEverythingIsInMemory) {
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		PresetManager manager(
			path,
			withFlag(fiftyoneDegreesHashInMemoryConfig, true),
			someNames.c_str());
		PresetDataSet dataSet(&manager.manager);
		EXPECT_STREQ(memoryType, dataSet->strings->typeName) << path;
		EXPECT_STREQ(memoryType, dataSet->values->typeName) << path;
	}
}

/**
 * Prints the bytes allocated for the data set with and without the setting,
 * naming some properties, as the allocations of the library report them.
 */
TEST_F(HashPresetTests, ReportsTheMemoryUsed) {
	for (const std::string& path : paths) {
		if (findNames(path) == false) {
			continue;
		}
		size_t steady[2] = { 0, 0 };
		size_t peak[2] = { 0, 0 };
		for (int on = 0; on < 2; on++) {
			fiftyoneDegreesSetUpMemoryTracking();
			{
				PresetManager manager(
					path,
					withFlag(fiftyoneDegreesHashHighPerformanceConfig, on == 1),
					someNames.c_str());
				steady[on] = fiftyoneDegreesMemoryTrackingGetAllocated();
				peak[on] = fiftyoneDegreesMemoryTrackingGetMax();
			}
			EXPECT_EQ(0u, fiftyoneDegreesUnsetMemoryTracking()) << path;
		}
		printf("%s naming %s allocates %zu bytes (peak %zu) without the "
			"setting and %zu bytes (peak %zu) with it\n",
			path.c_str(),
			someNames.c_str(),
			steady[0],
			peak[0],
			steady[1],
			peak[1]);
		EXPECT_LT(steady[1], steady[0]) << path;
	}
}
