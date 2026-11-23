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
 * Other tests cut the file part way through the values of a profile, so the
 * profile's header can be read but the rest cannot, and check that the read
 * leaves nothing allocated.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/hash.h"
#include "../../../src/common/collectionKeyTypes.h"

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

	// Gets the collection that reads the profiles from the file, which is
	// the source of the cache when the profiles are cached.
	static const fiftyoneDegreesCollectionFile* profilesFile(
		const fiftyoneDegreesDataSetHash* dataSet,
		const fiftyoneDegreesConfigHash& config) {
		const fiftyoneDegreesCollection* profiles = dataSet->profiles;
		if (config.profiles.capacity > 0) {
			profiles = ((const fiftyoneDegreesCollectionCache*)
				profiles->state)->source;
		}
		return (const fiftyoneDegreesCollectionFile*)profiles->state;
	}

	// Cuts the file part way through the values of a profile in the
	// results, then reads that profile from the file, through the profiles
	// collection, and through the values of every property.
	void readAfterProfileCut(fiftyoneDegreesConfigHash config) {
		init(config);
		fiftyoneDegreesResultsHash* results = detect();
		const fiftyoneDegreesDataSetHash* dataSet =
			(const fiftyoneDegreesDataSetHash*)results->b.b.dataSet;
		const fiftyoneDegreesCollectionFile* file =
			profilesFile(dataSet, config);

		// Walk the graph of every component, which sets the profile offsets
		// in the results without reading the profiles.
		{
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsEvaluateAll(&results->b.b, exception);
			ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		}

		// Find a profile in the results with at least two values. Its header
		// is read with a stream of its own, so nothing the data set reads
		// with next is already buffered.
		uint32_t profileOffset = UINT32_MAX;
		std::ifstream stream(copyPath, std::ios::binary);
		for (uint32_t i = 0;
			i < dataSet->componentsList.count &&
			profileOffset == UINT32_MAX;
			i++) {
			const uint32_t offset = results->items[0].profileOffsets[i];
			if (offset == UINT32_MAX) {
				continue;
			}
			byte header[sizeof(fiftyoneDegreesProfile)];
			stream.seekg((std::streamoff)(file->offset + offset));
			stream.read((char*)header, sizeof(header));
			ASSERT_TRUE(stream.good());
			if (((const fiftyoneDegreesProfile*)header)->valueCount >= 2) {
				profileOffset = offset;
			}
		}
		stream.close();
		ASSERT_NE(UINT32_MAX, profileOffset) <<
			"No profile with two or more values was found.";

		// Leave the profile's header and the first of its values.
		std::filesystem::resize_file(
			copyPath,
			(uintmax_t)file->offset + profileOffset +
			sizeof(fiftyoneDegreesProfile) + sizeof(uint32_t));
		const fiftyoneDegreesCollectionKey key = {
			{ profileOffset },
			CollectionKeyType_Profile,
		};

		// Read the profile into data the test provides. After the failure
		// the data must hold nothing, as the memory the read allocated has
		// been freed.
		{
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesData data;
			fiftyoneDegreesDataReset(&data);
			EXPECT_EQ(nullptr, file->read(file, &key, &data, exception)) <<
				"A profile whose values could not be read was returned.";
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_COLLECTION_FILE_READ_FAIL,
				exception->status);
			EXPECT_EQ(nullptr, data.ptr) <<
				"The data still points to the memory the failed read freed.";
			EXPECT_EQ(0U, data.allocated) <<
				"The data still counts the memory the failed read freed.";
			EXPECT_EQ(0U, data.used);
		}

		// Get the profile from the collection the data set uses, which reads
		// it from the file or loads it into the cache, and release it. The
		// second get loads it again.
		for (int i = 0; i < 2; i++) {
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			EXPECT_EQ(nullptr, dataSet->profiles->get(
				dataSet->profiles,
				&key,
				&item,
				exception)) <<
				"A profile whose values could not be read was returned.";
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_COLLECTION_FILE_READ_FAIL,
				exception->status);
			EXPECT_EQ(nullptr, item.data.ptr);
			EXPECT_EQ(0U, item.data.allocated);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->profiles, &item);
		}

		// Read the values of every property, as the results do.
		EXPECT_GT(readEveryProperty(results), 0) <<
			"Reads of the profile that was cut should report a failure.";
		fiftyoneDegreesResultsHashFree(results);
	}
};

TEST_F(HashFailedReadTests, LowMemory_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesHashLowMemoryConfig);
}

TEST_F(HashFailedReadTests, Balanced_FileCut_ReadsReportTheFailure) {
	readAfterCut(fiftyoneDegreesHashBalancedConfig);
}

TEST_F(HashFailedReadTests, LowMemory_ProfileCut_ReadsReportTheFailure) {
	readAfterProfileCut(fiftyoneDegreesHashLowMemoryConfig);
}

TEST_F(HashFailedReadTests, CachedProfiles_ProfileCut_ReadsReportTheFailure) {
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
	config.profiles.capacity = 100;
	readAfterProfileCut(config);
}
