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
 * @brief Tests that verify proper handling of data files with invalid versions.
 * 
 * These tests ensure that fiftyoneDegreesIpiInitManagerFromFile and
 * fiftyoneDegreesIpiInitManagerFromMemory correctly return 
 * FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION when given a data file/buffer
 * with an unsupported version (e.g., version 1.0 instead of the expected 4.4).
 * 
 * This validates the fix from commit 235443b that prevents SEGFAULT on
 * invalid data file versions by properly initializing graphsArray to NULL
 * and checking it before freeing.
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
#include "../src/ipi.h"
#include "../src/fiftyone.h"
}

/**
 * Size of the fake header we create (just the version fields).
 * The actual header is larger, but we only need enough to trigger
 * the version check.
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
 * Test fixture for invalid version tests.
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
	 * @return true if successful, false otherwise.
	 */
	bool writeBufferToTempFile(char* filePath, size_t filePathSize) {
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
		
		size_t written = fwrite(buffer, 1, TOTAL_BUFFER_SIZE, file);
		fclose(file);
		
		return written == TOTAL_BUFFER_SIZE;
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
 * This specifically tests the fix from commit 235443b.
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
