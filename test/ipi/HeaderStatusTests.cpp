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
 * @file HeaderStatusTests.cpp
 * @brief Tests that a data file whose header the API cannot use reports a
 * status that points at the data file rather than at the configuration.
 *
 * Each test builds a version 4.6 header in which every collection is empty and
 * sits straight after it, then changes one thing, so the failure the test
 * looks for is the first one reached.
 */

#include "pch.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/fiftyone.h"
}

class HeaderStatusTests : public ::testing::Test {
protected:
	typedef fiftyoneDegreesDataSetIpiHeader IpiHeader;

	std::vector<unsigned char> data;
	fiftyoneDegreesPropertiesRequired properties;
	std::string fileName;

	void SetUp() override {
		memset(&properties, 0, sizeof(properties));
		fileName = std::string("header-status-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".tmp";
		data.assign(sizeof(IpiHeader) + 64, 0);
		setInt32(offsetof(IpiHeader, versionMajor), 4);
		setInt32(offsetof(IpiHeader, versionMinor), 6);
		const size_t collections[] = {
			offsetof(IpiHeader, strings),
			offsetof(IpiHeader, components),
			offsetof(IpiHeader, maps),
			offsetof(IpiHeader, properties),
			offsetof(IpiHeader, values),
			offsetof(IpiHeader, profiles),
			offsetof(IpiHeader, graphs),
			offsetof(IpiHeader, profileGroups),
			offsetof(IpiHeader, propertyTypes),
			offsetof(IpiHeader, profileOffsets)
		};
		for (const size_t collection : collections) {
			setCollection(collection, sizeof(IpiHeader), 0);
		}
	}

	void setInt32(size_t offset, int32_t value) {
		memcpy(data.data() + offset, &value, sizeof(value));
	}

	void setCollection(size_t offset, size_t start, uint32_t length) {
		fiftyoneDegreesCollectionHeader header;
		header.startPosition = (fiftyoneDegreesFileOffsetUnsigned)start;
		header.length = length;
		header.count = 0;
		memcpy(data.data() + offset, &header, sizeof(header));
	}

	fiftyoneDegreesStatusCode initFromMemory() {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		return status;
	}

	fiftyoneDegreesStatusCode initFromFile(fiftyoneDegreesConfigIpi config) {
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(
				fileName.c_str(),
				data.data(),
				data.size()));
		fiftyoneDegreesResourceManager manager;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				fileName.c_str(),
				exception);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileDelete(fileName.c_str()));
		return status;
	}

	static void expectMessageContains(
		fiftyoneDegreesStatusCode status,
		const char *expected,
		const char *notExpected) {
		const char *message = fiftyoneDegreesStatusGetMessage(status, "file");
		ASSERT_NE(nullptr, message);
		EXPECT_NE(nullptr, strstr(message, expected)) << message;
		EXPECT_EQ(nullptr, strstr(message, notExpected)) << message;
		fiftyoneDegreesFree((void*)message);
	}
};

/**
 * A shift outside the range the API accepts is reported as an unsupported
 * version on every route into the engine, whose message does not send the
 * reader to check the file is uncompressed.
 */
TEST_F(HeaderStatusTests, ShiftOutsideRange) {
	const int32_t shifts[] = { -1, 9, INT32_MAX };
	for (const int32_t shift : shifts) {
		setInt32(offsetof(IpiHeader, profilesOffsetShift), shift);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION, initFromMemory())
			<< "memory, shift " << shift;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
			initFromFile(fiftyoneDegreesIpiInMemoryConfig))
			<< "file into memory, shift " << shift;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
			initFromFile(fiftyoneDegreesIpiLowMemoryConfig))
			<< "file, shift " << shift;
	}
	expectMessageContains(
		FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
		"unsupported version",
		"uncompressed");
}

/**
 * A collection the header makes longer than the memory is a fault in the data.
 */
TEST_F(HeaderStatusTests, CollectionBeyondMemory) {
	setCollection(offsetof(IpiHeader, strings), sizeof(IpiHeader), 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, initFromMemory());
}

/**
 * A collection that does not start where the one before it ended is a fault
 * in the data.
 */
TEST_F(HeaderStatusTests, CollectionOutOfStepInMemory) {
	setCollection(offsetof(IpiHeader, strings), sizeof(IpiHeader) + 1, 0);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, initFromMemory());
}

/**
 * A collection loaded from the file that the header makes longer than the file
 * is a fault in the file, not in the configuration.
 */
TEST_F(HeaderStatusTests, CollectionBeyondFile) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	config.strings.loaded = true;
	setCollection(offsetof(IpiHeader, strings), sizeof(IpiHeader), 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, initFromFile(config));
}

/**
 * A collection that starts beyond the end of the file is a fault in the file.
 */
TEST_F(HeaderStatusTests, CollectionStartBeyondFile) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	config.strings.loaded = true;
	setCollection(offsetof(IpiHeader, strings), 1024 * 1024, 16);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, initFromFile(config));
}

/**
 * A profiles collection whose length fits the file as a count of bytes, but
 * not once the shift the header declares is applied, is a fault in the file.
 */
TEST_F(HeaderStatusTests, ShiftedProfilesBeyondFile) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	config.profiles.loaded = true;
	setInt32(offsetof(IpiHeader, profilesOffsetShift), 3);
	setCollection(offsetof(IpiHeader, profiles), sizeof(IpiHeader), 32);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, initFromFile(config));
	expectMessageContains(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"complete and uncompressed",
		"configuration");
}

/**
 * A cache too small for its concurrency, on a file whose header is sound, is
 * still reported as a fault in the configuration.
 */
TEST_F(HeaderStatusTests, CacheTooSmallForConcurrency) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	config.strings.capacity = 1;
	config.strings.concurrency = 2;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INVALID_COLLECTION_CONFIG,
		initFromFile(config));
	expectMessageContains(
		FIFTYONE_DEGREES_STATUS_INVALID_COLLECTION_CONFIG,
		"configuration",
		"uncompressed");
}
