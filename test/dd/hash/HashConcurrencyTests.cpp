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
 * @file HashConcurrencyTests.cpp
 * @brief Checks that Concurrency defaults to the processor count, that a
 * performance profile keeps it, that the file pool has a handle for each
 * operation up to the most it opens, and that no value of it stops the
 * engine building.
 */

#include "../../common/pch.h"
#include <algorithm>
#include <string>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/common/threading.h"
#include "../../../src/dd/hash/hash.h"
#include "../../../src/dd/hash/ConfigHash.hpp"

using namespace FiftyoneDegrees::DeviceDetection::Hash;

// Concurrency set on a configuration before a performance profile is chosen.
static const uint16_t setConcurrencyBeforeProfile = 3;

// Concurrency too high for every shard of the Balanced values cache of 500
// to hold a node for each operation.
static const uint16_t highConcurrency = 100;

static const char concurrencyUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
	"AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 "
	"Safari/604.1";

class HashConcurrencyTests : public Base {
protected:
	std::string path;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;

	void SetUp() override {
		Base::SetUp();
		for (int i = _HashFileNamesLength - 1; i >= 0 && path.empty(); i--) {
			path = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		ASSERT_FALSE(path.empty()) << "No data file was found.";
	}

	void TearDown() override {
		if (managerReady) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(path);
		Base::TearDown();
	}

	fiftyoneDegreesStatusCode init(fiftyoneDegreesConfigHash* config) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				config,
				&properties,
				path.c_str(),
				exception);
		managerReady = status == FIFTYONE_DEGREES_STATUS_SUCCESS;
		return status;
	}
};

TEST_F(HashConcurrencyTests, DefaultConfig_IsTheProcessorCount) {
	ConfigHash config;
	EXPECT_EQ(
		fiftyoneDegreesThreadingGetProcessorCount(),
		config.getConcurrency());
}

TEST(HashConfigConcurrency, SetConcurrency_ReachesEveryCollection) {
	ConfigHash config;
	const uint16_t concurrency =
		static_cast<uint16_t>(config.getConcurrency() + 1);
	config.setConcurrency(concurrency);
	fiftyoneDegreesConfigHash *c = config.getConfig();
	EXPECT_EQ(concurrency, c->strings.concurrency);
	EXPECT_EQ(concurrency, c->components.concurrency);
	EXPECT_EQ(concurrency, c->maps.concurrency);
	EXPECT_EQ(concurrency, c->properties.concurrency);
	EXPECT_EQ(concurrency, c->values.concurrency);
	EXPECT_EQ(concurrency, c->profiles.concurrency);
	EXPECT_EQ(concurrency, c->rootNodes.concurrency);
	EXPECT_EQ(concurrency, c->nodes.concurrency);
	EXPECT_EQ(concurrency, c->profileOffsets.concurrency);
}

TEST_F(HashConcurrencyTests, PerformanceProfile_KeepsTheConcurrency) {
	ConfigHash config;
	config.setConcurrency(setConcurrencyBeforeProfile);
	config.setLowMemory();
	EXPECT_EQ(setConcurrencyBeforeProfile, config.getConcurrency());
	config.setBalanced();
	EXPECT_EQ(setConcurrencyBeforeProfile, config.getConcurrency());
	config.setBalancedTemp();
	EXPECT_EQ(setConcurrencyBeforeProfile, config.getConcurrency());
	config.setHighPerformance();
	EXPECT_EQ(setConcurrencyBeforeProfile, config.getConcurrency());
	config.setMaxPerformance();
	EXPECT_EQ(setConcurrencyBeforeProfile, config.getConcurrency());
}

TEST_F(HashConcurrencyTests, DefaultConfig_FilePoolFollowsTheProcessorCount) {
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashBalancedConfig;
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, init(&config));
	fiftyoneDegreesDataSetHash* dataSet = fiftyoneDegreesDataSetHashGet(&manager);
	EXPECT_EQ(
		std::min<uint16_t>(
			fiftyoneDegreesThreadingGetProcessorCount(),
			FIFTYONE_DEGREES_FILE_POOL_MAX_HANDLES),
		dataSet->b.b.filePool.pool.count);
	EXPECT_EQ(
		fiftyoneDegreesThreadingGetProcessorCount(),
		dataSet->config.values.concurrency);
	fiftyoneDegreesDataSetHashRelease(dataSet);
}

TEST_F(HashConcurrencyTests, HighConcurrency_Builds) {
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashBalancedConfig;
	config.strings.concurrency = highConcurrency;
	config.components.concurrency = highConcurrency;
	config.maps.concurrency = highConcurrency;
	config.properties.concurrency = highConcurrency;
	config.values.concurrency = highConcurrency;
	config.profiles.concurrency = highConcurrency;
	config.rootNodes.concurrency = highConcurrency;
	config.nodes.concurrency = highConcurrency;
	config.profileOffsets.concurrency = highConcurrency;
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, init(&config));
	fiftyoneDegreesDataSetHash* dataSet = fiftyoneDegreesDataSetHashGet(&manager);
	EXPECT_EQ(
		FIFTYONE_DEGREES_FILE_POOL_MAX_HANDLES,
		dataSet->b.b.filePool.pool.count) <<
		"The file pool should open no more than the most handles.";
	fiftyoneDegreesDataSetHashRelease(dataSet);

	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&manager, 1, 0);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromUserAgent(
		results,
		concurrencyUserAgent,
		sizeof(concurrencyUserAgent) - 1,
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	char buffer[64];
	char separator[] = "|";
	EXPECT_GT(fiftyoneDegreesResultsHashGetValuesString(
		results,
		"IsMobile",
		buffer,
		sizeof(buffer),
		separator,
		exception), 0u);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesResultsHashFree(results);
}
