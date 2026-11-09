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
 * @file IpiConcurrencyTests.cpp
 * @brief Checks that Concurrency defaults to the processor count, that a
 * performance profile keeps it, that the file pool has a handle for each
 * operation up to the most it opens, and that no value of it stops the
 * engine building.
 */

#include <algorithm>
#include <string>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/common/threading.h"
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/ConfigIpi.hpp"

using namespace FiftyoneDegrees::IpIntelligence;

static const char concurrencyAsnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";
static const char concurrencyAddress[] = "8.8.8.8";

// Concurrency set on a configuration before a performance profile is chosen.
static const uint16_t setConcurrencyBeforeProfile = 3;

// Concurrency too high for every shard of a values cache of
// smallValuesCache items to hold a node for each operation.
static const uint16_t highConcurrency = 100;
static const uint32_t smallValuesCache = 500;

class IpiConcurrencyTests : public Base {
protected:
	std::string path;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;

	void SetUp() override {
		Base::SetUp();
		path = GetFilePath(_dataFolderName, concurrencyAsnFileName);
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

	fiftyoneDegreesStatusCode init(fiftyoneDegreesConfigIpi* config) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				config,
				&properties,
				path.c_str(),
				exception);
		managerReady = status == FIFTYONE_DEGREES_STATUS_SUCCESS;
		return status;
	}
};

TEST_F(IpiConcurrencyTests, DefaultConfig_IsTheProcessorCount) {
	ConfigIpi config;
	EXPECT_EQ(
		fiftyoneDegreesThreadingGetProcessorCount(),
		config.getConcurrency());
}

/**
 * Checks that a configuration holds the collections as the Balanced profile
 * does, which is the default the specification and the C library give.
 */
static void expectBalanced(ConfigIpi &config) {
	const fiftyoneDegreesConfigIpi &balanced =
		fiftyoneDegreesIpiBalancedConfig;
	EXPECT_EQ(balanced.b.allInMemory, config.getConfig().b.allInMemory);
	EXPECT_EQ(balanced.strings.loaded, config.getStrings().getLoaded());
	EXPECT_EQ(balanced.strings.capacity, config.getStrings().getCapacity());
	EXPECT_EQ(balanced.profiles.loaded, config.getProfiles().getLoaded());
	EXPECT_EQ(balanced.profiles.capacity, config.getProfiles().getCapacity());
	EXPECT_EQ(balanced.graphs.loaded, config.getGraphs().getLoaded());
	EXPECT_EQ(balanced.graphs.capacity, config.getGraphs().getCapacity());
	EXPECT_EQ(balanced.graph.loaded, config.getGraph().getLoaded());
	EXPECT_EQ(balanced.graph.capacity, config.getGraph().getCapacity());
}

TEST_F(IpiConcurrencyTests, DefaultConfig_IsBalanced) {
	ConfigIpi config;
	expectBalanced(config);
	ConfigIpi fromNull(nullptr);
	expectBalanced(fromNull);
}

TEST_F(IpiConcurrencyTests, PerformanceProfile_KeepsTheConcurrency) {
	ConfigIpi config;
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

TEST_F(IpiConcurrencyTests, DefaultConfig_FilePoolFollowsTheProcessorCount) {
	if (path.empty()) {
		GTEST_SKIP() << "The ASN data file was not found.";
	}
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, init(&config));
	fiftyoneDegreesDataSetIpi* dataSet = fiftyoneDegreesDataSetIpiGet(&manager);
	EXPECT_EQ(
		std::min<uint16_t>(
			fiftyoneDegreesThreadingGetProcessorCount(),
			FIFTYONE_DEGREES_FILE_POOL_MAX_HANDLES),
		dataSet->b.b.filePool.pool.count);
	EXPECT_EQ(
		fiftyoneDegreesThreadingGetProcessorCount(),
		dataSet->config.values.concurrency);
	fiftyoneDegreesDataSetIpiRelease(dataSet);
}

TEST_F(IpiConcurrencyTests, HighConcurrency_Builds) {
	if (path.empty()) {
		GTEST_SKIP() << "The ASN data file was not found.";
	}
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig;
	config.values.loaded = false;
	config.values.capacity = smallValuesCache;
	config.strings.concurrency = highConcurrency;
	config.components.concurrency = highConcurrency;
	config.maps.concurrency = highConcurrency;
	config.properties.concurrency = highConcurrency;
	config.values.concurrency = highConcurrency;
	config.profiles.concurrency = highConcurrency;
	config.graphs.concurrency = highConcurrency;
	config.profileGroups.concurrency = highConcurrency;
	config.profileOffsets.concurrency = highConcurrency;
	config.propertyTypes.concurrency = highConcurrency;
	config.graph.concurrency = highConcurrency;
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, init(&config));
	fiftyoneDegreesDataSetIpi* dataSet = fiftyoneDegreesDataSetIpiGet(&manager);
	EXPECT_EQ(
		FIFTYONE_DEGREES_FILE_POOL_MAX_HANDLES,
		dataSet->b.b.filePool.pool.count) <<
		"The file pool should open no more than the most handles.";
	fiftyoneDegreesDataSetIpiRelease(dataSet);

	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		concurrencyAddress,
		sizeof(concurrencyAddress) - 1,
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	char buffer[256];
	EXPECT_GT(fiftyoneDegreesResultsIpiGetValuesString(
		results,
		"AsnName",
		buffer,
		sizeof(buffer),
		"|",
		exception), 0u);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesResultsIpiFree(results);
}
