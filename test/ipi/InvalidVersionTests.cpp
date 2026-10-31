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
 * @file InvalidVersionTests.cpp
 * @brief Tests that verify proper handling of data files with invalid
 * versions, or too short to hold a header.
 * 
 * These tests ensure that fiftyoneDegreesIpiInitManagerFromFile and
 * fiftyoneDegreesIpiInitManagerFromMemory correctly return 
 * FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION when given a data file/buffer
 * with an unsupported version (e.g., version 1.0 rather than the version the
 * engine targets). The tests also ensure that data shorter than the header
 * returns FIFTYONE_DEGREES_STATUS_CORRUPT_DATA without anything reading past
 * its end.
 * 
 * A failed initialization must free the partly built data set without
 * crashing.
 */

#include "pch.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/fiftyone.h"
}

/**
 * Size of the data set header. Only the version fields are set, which is
 * enough to fail the version check.
 */
static const size_t HEADER_SIZE = sizeof(fiftyoneDegreesDataSetIpiHeader);

/**
 * Size of random data to append after the header.
 */
static const size_t RANDOM_DATA_SIZE = 64;

/**
 * Total size of the test buffer.
 */
static const size_t TOTAL_BUFFER_SIZE = HEADER_SIZE + RANDOM_DATA_SIZE;

/**
 * Test fixture for invalid version and short data tests.
 */
class InvalidVersionTests : public ::testing::Test {
protected:
	unsigned char* buffer;
	fiftyoneDegreesConfigIpi config;
	fiftyoneDegreesPropertiesRequired properties;
	
	void SetUp() override {
		// Allocate buffer for header + random data
		buffer = (unsigned char*)malloc(TOTAL_BUFFER_SIZE);
		ASSERT_NE(buffer, nullptr) << "Failed to allocate test buffer";
		
		// Zero out the entire buffer first
		memset(buffer, 0, TOTAL_BUFFER_SIZE);
		
		// Set version 1.0 in the header (first two int32_t fields)
		// versionMajor = 1, versionMinor = 0
		int32_t versionMajor = 1;
		int32_t versionMinor = 0;
		memcpy(buffer, &versionMajor, sizeof(int32_t));
		memcpy(buffer + sizeof(int32_t), &versionMinor, sizeof(int32_t));
		
		// Fill the rest with random data (after the header)
		srand((unsigned int)time(NULL));
		for (size_t i = HEADER_SIZE; i < TOTAL_BUFFER_SIZE; i++) {
			buffer[i] = (unsigned char)(rand() % 256);
		}
		
		// Initialize config with default in-memory settings
		config = fiftyoneDegreesIpiInMemoryConfig;
		
		// Initialize properties to empty/null
		memset(&properties, 0, sizeof(properties));
	}
	
	void TearDown() override {
		if (buffer != nullptr) {
			free(buffer);
			buffer = nullptr;
		}
	}
	
	/**
	 * Helper to write the buffer to a temporary file.
	 * @param filePath Output path where the file was written.
	 * @param length Number of bytes from the start of the buffer to write.
	 * @return true if successful, false otherwise.
	 */
	bool writeBufferToTempFile(
		char* filePath,
		size_t filePathSize,
		size_t length = TOTAL_BUFFER_SIZE) {
#ifdef _WIN32
#	ifdef _MSC_VER
    	UNREFERENCED_PARAMETER(filePath);
    	UNREFERENCED_PARAMETER(filePathSize);
#	endif
		// Windows: use tmpnam_s or GetTempFileName
		char tempDir[MAX_PATH];
		if (GetTempPathA(MAX_PATH, tempDir) == 0) {
			return false;
		}
		if (GetTempFileNameA(tempDir, "ipi", 0, filePath) == 0) {
			return false;
		}
#else
		// POSIX: use mkstemp
		strncpy(filePath, "/tmp/ipi_test_XXXXXX", filePathSize);
		int fd = mkstemp(filePath);
		if (fd == -1) {
			return false;
		}
		close(fd);
#endif
		
		FILE* file = fopen(filePath, "wb");
		if (file == nullptr) {
			return false;
		}
		
		size_t written = fwrite(buffer, 1, length, file);
		fclose(file);
		
		return written == length;
	}

	/**
	 * Initializes a manager from a copy of the first length bytes of the
	 * buffer. The copy is allocated at that length, so that reading past its
	 * end is an out of bounds read a sanitizer or an unmapped page can catch.
	 * @param length Number of bytes to copy.
	 * @param size Size of the memory given to the init method.
	 * @return the status the init method returned.
	 */
	fiftyoneDegreesStatusCode initFromMemoryCopy(
		size_t length,
		fiftyoneDegreesFileOffset size) {
		unsigned char* copy = (unsigned char*)malloc(length);
		if (copy == nullptr) {
			ADD_FAILURE() << "Failed to allocate test buffer";
			return FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY;
		}
		memcpy(copy, buffer, length);

		fiftyoneDegreesResourceManager manager;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				copy,
				size,
				exception);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		free(copy);
		return status;
	}
};

/**
 * Test that fiftyoneDegreesIpiInitManagerFromMemory returns
 * INCORRECT_VERSION status when given a buffer with version 1.0.
 */
TEST_F(InvalidVersionTests, InitFromMemory_InvalidVersion_ReturnsIncorrectVersionStatus) {
	fiftyoneDegreesResourceManager manager;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	
	// Attempt to initialize from memory with invalid version data
	fiftyoneDegreesStatusCode status = fiftyoneDegreesIpiInitManagerFromMemory(
		&manager,
		&config,
		&properties,
		buffer,
		TOTAL_BUFFER_SIZE,
		exception);
	
	// Should return INCORRECT_VERSION, not crash
	EXPECT_EQ(status, FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION)
		<< "Expected INCORRECT_VERSION status for version 1.0 data, got: " 
		<< status;
	
	// If somehow it succeeded (it shouldn't), clean up
	if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
}

/**
 * Test that fiftyoneDegreesIpiInitManagerFromFile returns
 * INCORRECT_VERSION status when given a file with version 1.0.
 */
TEST_F(InvalidVersionTests, InitFromFile_InvalidVersion_ReturnsIncorrectVersionStatus) {
	char tempFilePath[512];
	
	// Write the invalid version buffer to a temp file
	ASSERT_TRUE(writeBufferToTempFile(tempFilePath, sizeof(tempFilePath)))
		<< "Failed to create temporary test file";
	
	fiftyoneDegreesResourceManager manager;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	
	// Attempt to initialize from file with invalid version data
	fiftyoneDegreesStatusCode status = fiftyoneDegreesIpiInitManagerFromFile(
		&manager,
		&config,
		&properties,
		tempFilePath,
		exception);
	
	// Clean up the temp file
	remove(tempFilePath);
	
	// Should return INCORRECT_VERSION, not crash
	EXPECT_EQ(status, FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION)
		<< "Expected INCORRECT_VERSION status for version 1.0 data file, got: " 
		<< status;
	
	// If somehow it succeeded (it shouldn't), clean up
	if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
}

/**
 * Test that the manager is not left in an invalid state after
 * initialization fails due to incorrect version.
 */
TEST_F(InvalidVersionTests, InitFromMemory_InvalidVersion_NoResourceLeak) {
	fiftyoneDegreesResourceManager manager;
	memset(&manager, 0, sizeof(manager));
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	
	// Attempt to initialize - should fail cleanly
	fiftyoneDegreesStatusCode status = fiftyoneDegreesIpiInitManagerFromMemory(
		&manager,
		&config,
		&properties,
		buffer,
		TOTAL_BUFFER_SIZE,
		exception);
	
	EXPECT_EQ(status, FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
	
	// The manager should not have been initialized, so we should not
	// need to free it. This test passes if we don't crash.
}

/**
 * Test that fiftyoneDegreesIpiInitManagerFromMemory returns CORRUPT_DATA
 * status when the memory is shorter than the header, rather than reading the
 * header from past its end.
 */
TEST_F(InvalidVersionTests, InitFromMemory_ShorterThanHeader_ReturnsCorruptDataStatus) {
	const size_t lengths[] = { 1, HEADER_SIZE - 1 };
	for (size_t length : lengths) {
		EXPECT_EQ(
			initFromMemoryCopy(length, (fiftyoneDegreesFileOffset)length),
			FIFTYONE_DEGREES_STATUS_CORRUPT_DATA)
			<< "Expected CORRUPT_DATA status for " << length
			<< " bytes of memory";
	}
}

/**
 * Test that a negative size, which puts the end of the memory before its
 * start, returns CORRUPT_DATA status rather than reading a header.
 */
TEST_F(InvalidVersionTests, InitFromMemory_NegativeSize_ReturnsCorruptDataStatus) {
	EXPECT_EQ(
		initFromMemoryCopy(1, -1),
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA);
}

/**
 * Test that memory holding exactly a header passes the length check and
 * fails on the version instead.
 */
TEST_F(InvalidVersionTests, InitFromMemory_HeaderOnly_ReturnsIncorrectVersionStatus) {
	EXPECT_EQ(
		initFromMemoryCopy(
			HEADER_SIZE,
			(fiftyoneDegreesFileOffset)HEADER_SIZE),
		FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
}

/**
 * Test that fiftyoneDegreesIpiInitManagerFromFile returns CORRUPT_DATA status
 * when the file is shorter than the header. The in memory configuration reads
 * the file into memory of the same length and takes the header from there.
 */
TEST_F(InvalidVersionTests, InitFromFile_ShorterThanHeader_ReturnsCorruptDataStatus) {
	const size_t lengths[] = { 1, HEADER_SIZE - 1 };
	for (size_t length : lengths) {
		char tempFilePath[512];
		ASSERT_TRUE(writeBufferToTempFile(
			tempFilePath,
			sizeof(tempFilePath),
			length))
			<< "Failed to create temporary test file";

		fiftyoneDegreesResourceManager manager;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				tempFilePath,
				exception);
		remove(tempFilePath);

		EXPECT_EQ(status, FIFTYONE_DEGREES_STATUS_CORRUPT_DATA)
			<< "Expected CORRUPT_DATA status for a " << length
			<< " byte data file, got: " << status;
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
	}
}
