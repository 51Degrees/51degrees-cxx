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
 * @file EngineIpiInitTests.cpp
 * @brief Tests that the engine constructor from memory frees the copy it takes
 * of the memory when the data set cannot be created, and that a configuration
 * holds its own values.
 *
 * Each test builds the memory it offers, so no data file is needed.
 */

#include "pch.h"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "IpiTestMemory.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

class EngineIpiInitTests : public Base {
protected:
	static void expectThrows(
		TestMemory &memory,
		fiftyoneDegreesStatusCode expected) {
		ConfigIpi config;
		RequiredPropertiesConfig properties;
		try {
			EngineIpi engine(
				memory.data(),
				(fiftyoneDegreesFileOffset)memory.size(),
				&config,
				&properties);
			ADD_FAILURE() << "No exception was thrown";
		}
		catch (StatusCodeException &e) {
			EXPECT_EQ(expected, e.getCode());
		}
	}

	/**
	 * Expects the constructor to throw the status provided and to leave
	 * nothing allocated. It runs with tracking, which every build can check,
	 * then without, as the address sanitizer cannot see a leak while tracking
	 * holds a pointer to the memory.
	 */
	static void expectFreesCopy(
		TestMemory &memory,
		fiftyoneDegreesStatusCode expected) {
		fiftyoneDegreesSetUpMemoryTracking();
		expectThrows(memory, expected);
		EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
		expectThrows(memory, expected);
	}
};

/**
 * Test that a configuration made from no source can be used and deleted.
 */
TEST_F(EngineIpiInitTests, Config_NoSource_HoldsItsOwnValues) {
	ConfigIpi *config = new ConfigIpi(nullptr);
	config->setUseTempFile(true);
	EXPECT_TRUE(config->getUseTempFile());
	EXPECT_TRUE(config->getConfig().b.useTempFile);
	delete config;
}

/**
 * Test that changing a configuration leaves the source it was copied from alone.
 */
TEST_F(EngineIpiInitTests, Config_Source_IsCopied) {
	const bool before = fiftyoneDegreesIpiBalancedConfig.b.useTempFile;
	ConfigIpi config(&fiftyoneDegreesIpiBalancedConfig);
	config.setUseTempFile(!before);
	const bool source = fiftyoneDegreesIpiBalancedConfig.b.useTempFile;
	fiftyoneDegreesIpiBalancedConfig.b.useTempFile = before;
	EXPECT_EQ(before, source);
	EXPECT_EQ(!before, config.getConfig().b.useTempFile);
}

/**
 * Test that the copy is freed when the init method returns a failed status.
 */
TEST_F(EngineIpiInitTests, Garbage_Memory_FreesCopy) {
	TestMemory memory = garbage(64 * 1024);
	expectFreesCopy(memory, FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
}

/**
 * Test that the copy is freed when only the exception shows the failure, as
 * the init method returns success with no data set.
 */
TEST_F(EngineIpiInitTests, ComponentMissing_Memory_FreesCopy) {
	TestMemory memory = emptyDataSet();
	setCollection(
		memory,
		offsetof(TestHeader, components),
		sizeof(TestHeader),
		0,
		1);
	expectFreesCopy(
		memory,
		FIFTYONE_DEGREES_STATUS_COLLECTION_OFFSET_OUT_OF_RANGE);
}
