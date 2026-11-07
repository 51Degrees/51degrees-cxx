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
 * @file IpiFailedReadTests.cpp
 * @brief Cuts a copy of the data file short after a lookup, then reads every
 * property with the calls the C++ results make. Each read of data the file
 * no longer holds must report the failure rather than use the data.
 */

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/ipi.h"

static const char failedReadAsnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";
static const char failedReadAddress[] = "8.8.8.8";

// Bytes of the data file left after the cut.
static const uintmax_t failedReadCutBytes = 4096;

class IpiFailedReadTests : public Base {
protected:
	std::string copyPath;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;

	void SetUp() override {
		Base::SetUp();
		const std::string found =
			GetFilePath(_dataFolderName, failedReadAsnFileName);
		if (found.empty()) {
			GTEST_SKIP() << "The ASN data file was not found.";
		}
		copyPath = std::string("ipi-failed-read-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".ipi";
		std::filesystem::copy_file(
			found,
			copyPath,
			std::filesystem::copy_options::overwrite_existing);
	}

	void TearDown() override {
		if (managerReady) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		if (copyPath.empty() == false) {
			std::remove(copyPath.c_str());
		}
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(copyPath);
		Base::TearDown();
	}

	void init(fiftyoneDegreesConfigIpi config) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				copyPath.c_str(),
				exception));
		managerReady = true;
	}

	fiftyoneDegreesResultsIpi* lookUp() {
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			failedReadAddress,
			sizeof(failedReadAddress) - 1,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		return results;
	}

	// Reads every property, returning the number of reads that reported a
	// failure. The values returned by a read that succeeded are read too.
	int readEveryProperty(fiftyoneDegreesResultsIpi* results) {
		const fiftyoneDegreesDataSetIpi* dataSet =
			(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
		const int count = (int)dataSet->b.b.available->count;
		int failures = 0;
		std::vector<char> buffer(4096);
		for (int i = 0; i < count; i++) {
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsIpiGetHasValues(results, i, exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			fiftyoneDegreesResultsIpiGetNoValueReason(results, i, exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			const fiftyoneDegreesProfilePercentage* items =
				fiftyoneDegreesResultsIpiGetValues(results, i, exception);
			if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
				failures++;
			}
			else if (items != nullptr) {
				for (uint32_t v = 0; v < results->values.count; v++) {
					const void* value = items[v].item.data.ptr;
					if (value != nullptr) {
						volatile char first = *(const char*)value;
						(void)first;
					}
				}
			}
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			const char* name = FIFTYONE_DEGREES_STRING(
				fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
					dataSet->b.b.available,
					i));
			fiftyoneDegreesResultsIpiGetValuesString(
				results,
				name,
				buffer.data(),
				buffer.size(),
				"|",
				exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
		}
		return failures;
	}

	void readAfterCut(fiftyoneDegreesConfigIpi config) {
		init(config);
		fiftyoneDegreesResultsIpi* results = lookUp();
		std::filesystem::resize_file(copyPath, failedReadCutBytes);
		EXPECT_GT(readEveryProperty(results), 0) <<
			"Reads of data the file no longer holds should report a failure.";
		fiftyoneDegreesResultsIpiFree(results);
	}
};

TEST_F(IpiFailedReadTests, LowMemory_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesIpiLowMemoryConfig);
}

TEST_F(IpiFailedReadTests, Balanced_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesIpiBalancedConfig);
}
