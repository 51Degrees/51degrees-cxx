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
 * @file HashConcurrentReadTests.cpp
 * @brief Runs more threads than the concurrency in profiles that read the
 * data file. Every detection must complete and every property must read
 * without a failure, and in LowMemory the file pool must have made threads
 * wait for its one handle.
 */

#include "../../common/pch.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/common/threading.h"
#include "../../../src/dd/hash/hash.h"

// Threads that share the data set, more than its one file handle.
static const int concurrentReadThreads = 8;

// Detections each thread makes.
static const int concurrentReadDetections = 25;

static const char* concurrentReadUserAgents[] = {
	"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
	"AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 "
	"Safari/604.1",
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
	"Mozilla/5.0 (Linux; Android 14; SM-S918B) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36",
	"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
	"(KHTML, like Gecko) Version/17.1 Safari/605.1.15"
};

class HashConcurrentReadTests : public Base {
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

	// Gives every collection a concurrency of one, so the file pool has one
	// handle for all the threads.
	static void oneHandle(fiftyoneDegreesConfigHash* config) {
		config->strings.concurrency = 1;
		config->components.concurrency = 1;
		config->maps.concurrency = 1;
		config->properties.concurrency = 1;
		config->values.concurrency = 1;
		config->profiles.concurrency = 1;
		config->rootNodes.concurrency = 1;
		config->nodes.concurrency = 1;
		config->profileOffsets.concurrency = 1;
	}

	// Detects each User-Agent in turn and reads every property with the calls
	// the C++ results make, counting the reads that failed and the results
	// that gave IsMobile a value.
	void detectAndRead(
		std::atomic<int>* failures,
		std::atomic<int>* withValues) {
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&manager, 1, 0);
		const fiftyoneDegreesDataSetHash* dataSet =
			(const fiftyoneDegreesDataSetHash*)results->b.b.dataSet;
		const int count = (int)dataSet->b.b.available->count;
		std::vector<char> buffer(4096);
		char separator[] = "|";
		const int userAgents = (int)(sizeof(concurrentReadUserAgents) /
			sizeof(concurrentReadUserAgents[0]));
		for (int d = 0; d < concurrentReadDetections; d++) {
			const char* userAgent = concurrentReadUserAgents[d % userAgents];
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsHashFromUserAgent(
				results,
				userAgent,
				strlen(userAgent),
				exception);
			*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			for (int i = 0; i < count; i++) {
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsHashGetHasValues(results, i, exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsHashGetNoValueReason(
					results,
					i,
					exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsHashGetValues(results, i, exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			}
			FIFTYONE_DEGREES_EXCEPTION_CLEAR;
			const size_t added = fiftyoneDegreesResultsHashGetValuesString(
				results,
				"IsMobile",
				buffer.data(),
				buffer.size(),
				separator,
				exception);
			*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			*withValues += added > 0 ? 1 : 0;
		}
		fiftyoneDegreesResultsHashFree(results);
	}

	// Runs the threads and returns the number of times the file pool made a
	// thread wait for its handle.
	long readConcurrently(fiftyoneDegreesConfigHash config) {
		oneHandle(&config);
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path.c_str(),
				exception));
		if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
			return 0;
		}
		managerReady = true;

		std::atomic<int> failures(0);
		std::atomic<int> withValues(0);
		std::vector<std::thread> threads;
		for (int t = 0; t < concurrentReadThreads; t++) {
			threads.emplace_back(
				&HashConcurrentReadTests::detectAndRead,
				this,
				&failures,
				&withValues);
		}
		for (std::thread& thread : threads) {
			thread.join();
		}

		fiftyoneDegreesDataSetHash* dataSet =
			fiftyoneDegreesDataSetHashGet(&manager);
		EXPECT_EQ(1, dataSet->b.b.filePool.pool.count) <<
			"The file pool should have one handle for every thread.";
		const long waits = dataSet->b.b.filePool.pool.waits;
		fiftyoneDegreesDataSetHashRelease(dataSet);
		printf("The file pool made threads wait %ld times\n", waits);

		EXPECT_EQ(0, failures.load()) <<
			"Every detection and read should complete without a failure.";
		EXPECT_EQ(
			concurrentReadThreads * concurrentReadDetections,
			withValues.load()) << "Every detection should give IsMobile a value.";
		return waits;
	}
};

TEST_F(HashConcurrentReadTests, LowMemory_MoreThreadsThanHandles_EveryReadCompletes) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		GTEST_SKIP() << "A single threaded build has no threads to share.";
	}
	EXPECT_GT(readConcurrently(fiftyoneDegreesHashLowMemoryConfig), 0) <<
		"LowMemory reads everything from the file, so threads should have "
		"waited for the one handle.";
}

TEST_F(HashConcurrentReadTests, Balanced_MoreThreadsThanHandles_EveryReadCompletes) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		GTEST_SKIP() << "A single threaded build has no threads to share.";
	}
	readConcurrently(fiftyoneDegreesHashBalancedConfig);
}
