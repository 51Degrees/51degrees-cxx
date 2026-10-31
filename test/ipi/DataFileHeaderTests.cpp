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
 * @file DataFileHeaderTests.cpp
 * @brief Tests that the engine reads a data file with a non zero profile
 * offset shift, and refuses a copy whose header it does not support.
 *
 * The tests use the ASN data file because it is committed to the data
 * submodule, so every build reads the same file. CI replaces the other data
 * files the tests use. Only the header of a copy is changed, because the
 * offsets stored in the file are in the unit its header declares, so another
 * valid shift needs a file written for that shift.
 */

#include "../common/Base.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../../src/ipi/ipi.h"
#include "../../src/ipi/fiftyone.h"
#include "Constants.hpp"

static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

static const size_t versionMinorPosition =
	offsetof(fiftyoneDegreesDataSetIpiHeader, versionMinor);

static const size_t shiftPosition =
	offsetof(fiftyoneDegreesDataSetIpiHeader, profilesOffsetShift);

/**
 * The status for a shift outside the range the header can declare.
 */
static const fiftyoneDegreesStatusCode invalidShiftStatus =
	FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION;

/**
 * Test fixture holding a copy of the ASN data file in memory, which each test
 * can change and then offer to the engine from memory or from a file.
 */
class DataFileHeaderTests : public Base {
protected:
	std::vector<unsigned char> data;
	std::string tempFilePath;

	void SetUp() override {
		Base::SetUp();
		const std::string path = GetFilePath(_dataFolderName, asnFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << asnFileName << "' was not found. "
				"Run 'git submodule update --init' to fetch it.";
		}
		FILE* file = fopen(path.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
		fseek(file, 0, SEEK_END);
		data.resize((size_t)ftell(file));
		rewind(file);
		const size_t read = fread(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), read) << "Could not read '" << path << "'.";
		ASSERT_GT(data.size(), sizeof(fiftyoneDegreesDataSetIpiHeader));
	}

	void TearDown() override {
		if (tempFilePath.empty() == false) {
			EXPECT_EQ(0, remove(tempFilePath.c_str())) <<
				"Could not delete '" << tempFilePath << "'. A file handle "
				"may still be open.";
		}
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(data);
		std::string().swap(tempFilePath);
		Base::TearDown();
	}

	void setHeaderField(const size_t position, const int32_t value) {
		memcpy(data.data() + position, &value, sizeof(value));
	}

	int32_t getHeaderField(const size_t position) const {
		int32_t value;
		memcpy(&value, data.data() + position, sizeof(value));
		return value;
	}

	/**
	 * Writes the data to a file that only this test uses, in the system's
	 * temporary folder, because CTest can run tests in parallel. The system
	 * creates the name. TearDown deletes the file.
	 */
	void writeTempFile() {
		if (tempFilePath.empty()) {
#ifdef _WIN32
			char directory[MAX_PATH];
			char path[MAX_PATH];
			ASSERT_NE(0u, GetTempPathA(MAX_PATH, directory));
			ASSERT_NE(0u, GetTempFileNameA(directory, "ipi", 0, path));
#else
			char path[] = "/tmp/ipi_header_test_XXXXXX";
			const int descriptor = mkstemp(path);
			ASSERT_NE(-1, descriptor);
			close(descriptor);
#endif
			tempFilePath = path;
		}
		FILE* file = fopen(tempFilePath.c_str(), "wb");
		ASSERT_NE(nullptr, file) << "Could not open '" << tempFilePath << "'.";
		const size_t written = fwrite(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), written);
	}

	/**
	 * Gets the status from the data as it is now, passing the manager to the
	 * method provided when the data is accepted.
	 */
	template <typename Use>
	fiftyoneDegreesStatusCode initFromMemory(Use use) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception);
		return finishInit(status, exception, &manager, use);
	}

	/**
	 * Gets the status from the temporary file using the configuration
	 * provided, passing the manager to the method provided when the file is
	 * accepted.
	 */
	template <typename Use>
	fiftyoneDegreesStatusCode initFromFile(
		fiftyoneDegreesConfigIpi config,
		Use use) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				tempFilePath.c_str(),
				exception);
		return finishInit(status, exception, &manager, use);
	}

	/**
	 * Expects the status from every way the engine reads a data file. In a
	 * memory only build the only way is into memory.
	 */
	void expectStatus(const fiftyoneDegreesStatusCode expected) {
		ASSERT_NO_FATAL_FAILURE(writeTempFile());
		const auto ignore = [](fiftyoneDegreesResourceManager*) {};
		const int32_t shift = getHeaderField(shiftPosition);
		EXPECT_EQ(expected, initFromMemory(ignore)) <<
			"From memory with shift " << shift;
		EXPECT_EQ(expected, initFromFile(
			fiftyoneDegreesIpiInMemoryConfig, ignore)) <<
			"File into memory with shift " << shift;
		if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
			EXPECT_EQ(expected, initFromFile(
				fiftyoneDegreesIpiLowMemoryConfig, ignore)) <<
				"File streamed with shift " << shift;
		}
	}

	/**
	 * Gets the values of every property for an address, one line per property.
	 */
	static std::string getValues(
		fiftyoneDegreesResourceManager* manager,
		const char* ipAddress) {
		char buffer[1024];
		std::string values;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(manager);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			ipAddress,
			strlen(ipAddress),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << ipAddress;
		const fiftyoneDegreesDataSetIpi* dataSet =
			(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
		for (int i = 0; i < (int)dataSet->b.b.available->count; i++) {
			buffer[0] = '\0';
			fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
				results,
				i,
				buffer,
				sizeof(buffer),
				"|",
				exception);
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << ipAddress;
			values.append(buffer).append("\n");
		}
		fiftyoneDegreesResultsIpiFree(results);
		return values;
	}

private:
	template <typename Use>
	static fiftyoneDegreesStatusCode finishInit(
		const fiftyoneDegreesStatusCode status,
		fiftyoneDegreesException* exception,
		fiftyoneDegreesResourceManager* manager,
		Use use) {
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS &&
			FIFTYONE_DEGREES_EXCEPTION_OKAY) {
			use(manager);
			fiftyoneDegreesResourceManagerFree(manager);
		}
		return status;
	}
};

/**
 * Test that the file these tests rely on does declare a shift, so that the
 * tests that read it exercise the shifted paths. A new data file with no shift
 * fails here rather than quietly removing that coverage.
 */
TEST_F(DataFileHeaderTests, AsnFile_DeclaresNonZeroShift) {
	EXPECT_GT(getHeaderField(shiftPosition), 0);
}

/**
 * Test that an unchanged copy is accepted, which shows that the refusals below
 * come from the header fields they change.
 */
TEST_F(DataFileHeaderTests, UnchangedCopy_IsAccepted) {
	expectStatus(FIFTYONE_DEGREES_STATUS_SUCCESS);
}

/**
 * Test that every way of reading a shifted file returns the right values. Each
 * collection type converts stored offsets in its own code, so a fault in one
 * of them shows as a difference from the others. Both addresses are in an
 * autonomous system the file holds, so the lookups read profiles through their
 * stored offsets rather than returning a value that means no data.
 */
TEST_F(DataFileHeaderTests, UnchangedCopy_EveryConfigurationReturnsSameValues) {
	ASSERT_NO_FATAL_FAILURE(writeTempFile());
	const char* const addresses[] = { "1.1.1.1", "2606:4700:4700::1111" };
	for (const char* address : addresses) {
		std::string expected;
		const auto read = [address, &expected](
			fiftyoneDegreesResourceManager* manager) {
			expected = getValues(manager, address);
		};
		const auto compare = [address, &expected](
			fiftyoneDegreesResourceManager* manager) {
			EXPECT_EQ(expected, getValues(manager, address)) << address;
		};
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			initFromMemory(read)) << address;
		EXPECT_NE(std::string::npos, expected.find("13335")) <<
			address << " returned " << expected;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			initFromFile(fiftyoneDegreesIpiInMemoryConfig, compare)) << address;
		if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				initFromFile(fiftyoneDegreesIpiLowMemoryConfig, compare)) <<
				address;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				initFromFile(fiftyoneDegreesIpiBalancedConfig, compare)) <<
				address;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				initFromFile(fiftyoneDegreesIpiHighPerformanceConfig, compare)) <<
				address;
		}
	}
}

/**
 * Test that a shift above the largest the header can declare is refused. Nine
 * is the first value above the limit of eight.
 */
TEST_F(DataFileHeaderTests, ShiftAboveLimit_IsRefused) {
	const int32_t shifts[] = { 9, INT32_MAX };
	for (const int32_t shift : shifts) {
		setHeaderField(shiftPosition, shift);
		expectStatus(invalidShiftStatus);
	}
}

/**
 * Test that a negative shift is refused rather than converted to an unsigned
 * shift count.
 */
TEST_F(DataFileHeaderTests, NegativeShift_IsRefused) {
	const int32_t shifts[] = { -1, INT32_MIN };
	for (const int32_t shift : shifts) {
		setHeaderField(shiftPosition, shift);
		expectStatus(invalidShiftStatus);
	}
}

/**
 * Test that a version 4.5 file is refused. The field that holds the shift in
 * version 4.6 is always zero in version 4.5.
 */
TEST_F(DataFileHeaderTests, Version45_ReturnsIncorrectVersion) {
	setHeaderField(versionMinorPosition, 5);
	setHeaderField(shiftPosition, 0);
	expectStatus(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
}

/**
 * Test that the version is checked before the shift, so the field is not read
 * as a shift in a file that has no such field.
 */
TEST_F(DataFileHeaderTests, Version45WithInvalidShift_ReturnsIncorrectVersion) {
	setHeaderField(versionMinorPosition, 5);
	setHeaderField(shiftPosition, 9);
	expectStatus(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
}
