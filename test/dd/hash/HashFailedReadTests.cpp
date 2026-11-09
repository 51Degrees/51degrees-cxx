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
 * @file HashFailedReadTests.cpp
 * @brief Cuts a copy of the data file short after a detection, then reads
 * every property with the calls the C++ results make. Each read of data the
 * file no longer holds must report the failure rather than use the data.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/hash.h"

static const char failedReadUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
	"AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 "
	"Safari/604.1";

// Bytes of the data file left after the cut.
static const uintmax_t failedReadCutBytes = 4096;

class HashFailedReadTests : public Base {
protected:
	std::string copyPath;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;

	void SetUp() override {
		Base::SetUp();
		std::string found;
		for (int i = _HashFileNamesLength - 1; i >= 0 && found.empty(); i--) {
			found = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		ASSERT_FALSE(found.empty()) << "No data file was found.";
		copyPath = std::string("hash-failed-read-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".hash";
		std::filesystem::copy_file(
			found,
			copyPath,
			std::filesystem::copy_options::overwrite_existing);
	}

	void TearDown() override {
		if (managerReady) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		std::remove(copyPath.c_str());
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(copyPath);
		Base::TearDown();
	}

	void init(fiftyoneDegreesConfigHash config) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				copyPath.c_str(),
				exception));
		managerReady = true;
	}

	fiftyoneDegreesResultsHash* detect() {
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&manager, 1, 0);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			failedReadUserAgent,
			sizeof(failedReadUserAgent) - 1,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		return results;
	}

	// Reads every property, returning the number of reads that reported a
	// failure. The values returned by a read that succeeded are read too.
	int readEveryProperty(fiftyoneDegreesResultsHash* results) {
		const fiftyoneDegreesDataSetHash* dataSet =
			(const fiftyoneDegreesDataSetHash*)results->b.b.dataSet;
		const int count = (int)dataSet->b.b.available->count;
		int failures = 0;
		std::vector<char> buffer(4096);
		char separator[] = "|";
		for (int i = 0; i < count; i++) {
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsHashGetHasValues(results, i, exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			fiftyoneDegreesResultsHashGetNoValueReason(results, i, exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			fiftyoneDegreesCollectionItem* items =
				fiftyoneDegreesResultsHashGetValues(results, i, exception);
			if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
				failures++;
				EXPECT_EQ(nullptr, items) <<
					"Values were returned from a read that failed.";
			}
			else if (items != nullptr) {
				for (uint32_t v = 0; v < results->values.count; v++) {
					const fiftyoneDegreesString* value =
						(const fiftyoneDegreesString*)items[v].data.ptr;
					if (value != nullptr) {
						volatile char first = (&value->value)[0];
						(void)first;
					}
				}
			}
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			const char* name = FIFTYONE_DEGREES_STRING(
				fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
					dataSet->b.b.available,
					i));
			fiftyoneDegreesResultsHashGetValuesString(
				results,
				name,
				buffer.data(),
				buffer.size(),
				separator,
				exception);
			failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
		}
		return failures;
	}

	void readAfterCut(fiftyoneDegreesConfigHash config) {
		init(config);
		fiftyoneDegreesResultsHash* results = detect();
		std::filesystem::resize_file(copyPath, failedReadCutBytes);
		EXPECT_GT(readEveryProperty(results), 0) <<
			"Reads of data the file no longer holds should report a failure.";
		fiftyoneDegreesResultsHashFree(results);
	}
};

TEST_F(HashFailedReadTests, LowMemory_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesHashLowMemoryConfig);
}

TEST_F(HashFailedReadTests, Balanced_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesHashBalancedConfig);
}
