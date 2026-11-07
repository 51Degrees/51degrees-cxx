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
 * @file IpiConcurrentReadTests.cpp
 * @brief Runs more threads than the concurrency in profiles that read the
 * data file. Every lookup must complete and every property must read without
 * a failure, and in LowMemory the file pool must have made threads wait for
 * its one handle.
 */

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/common/threading.h"
#include "../../src/ipi/ipi.h"

static const char concurrentReadAsnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

// Threads that share the data set, more than its one file handle.
static const int concurrentReadThreads = 8;

// Lookups each thread makes.
static const int concurrentReadLookups = 25;

static const char* concurrentReadAddresses[] = {
	"8.8.8.8",
	"1.1.1.1",
	"2001:4860:4860::8888",
	"51.140.0.1"
};

class IpiConcurrentReadTests : public Base {
protected:
	std::string path;
	fiftyoneDegreesResourceManager manager;
	bool managerReady = false;

	void SetUp() override {
		Base::SetUp();
		path = GetFilePath(_dataFolderName, concurrentReadAsnFileName);
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
	static void oneHandle(fiftyoneDegreesConfigIpi* config) {
		config->strings.concurrency = 1;
		config->components.concurrency = 1;
		config->maps.concurrency = 1;
		config->properties.concurrency = 1;
		config->values.concurrency = 1;
		config->profiles.concurrency = 1;
		config->graphs.concurrency = 1;
		config->profileGroups.concurrency = 1;
		config->profileOffsets.concurrency = 1;
		config->propertyTypes.concurrency = 1;
		config->graph.concurrency = 1;
	}

	// Looks up each address in turn and reads every property with the calls
	// the C++ results make, counting the reads that failed and the values
	// read.
	void lookUpAndRead(
		std::atomic<int>* failures,
		std::atomic<int>* withValues) {
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		const fiftyoneDegreesDataSetIpi* dataSet =
			(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
		const int count = (int)dataSet->b.b.available->count;
		std::vector<char> buffer(4096);
		const int addresses = (int)(sizeof(concurrentReadAddresses) /
			sizeof(concurrentReadAddresses[0]));
		for (int l = 0; l < concurrentReadLookups; l++) {
			const char* address = concurrentReadAddresses[l % addresses];
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsIpiFromIpAddressString(
				results,
				address,
				strlen(address),
				exception);
			*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
			for (int i = 0; i < count; i++) {
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsIpiGetHasValues(results, i, exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsIpiGetNoValueReason(
					results,
					i,
					exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				fiftyoneDegreesResultsIpiGetValues(results, i, exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				const char* name = FIFTYONE_DEGREES_STRING(
					fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
						dataSet->b.b.available,
						i));
				const size_t added = fiftyoneDegreesResultsIpiGetValuesString(
					results,
					name,
					buffer.data(),
					buffer.size(),
					"|",
					exception);
				*failures += FIFTYONE_DEGREES_EXCEPTION_FAILED ? 1 : 0;
				*withValues += added > 0 ? 1 : 0;
			}
		}
		fiftyoneDegreesResultsIpiFree(results);
	}

	// Runs the threads and returns the number of times the file pool made a
	// thread wait for its handle.
	long readConcurrently(fiftyoneDegreesConfigIpi config) {
		oneHandle(&config);
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
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
				&IpiConcurrentReadTests::lookUpAndRead,
				this,
				&failures,
				&withValues);
		}
		for (std::thread& thread : threads) {
			thread.join();
		}

		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		EXPECT_EQ(1, dataSet->b.b.filePool.pool.count) <<
			"The file pool should have one handle for every thread.";
		const long waits = dataSet->b.b.filePool.pool.waits;
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		printf("The file pool made threads wait %ld times\n", waits);

		EXPECT_EQ(0, failures.load()) <<
			"Every lookup and read should complete without a failure.";
		EXPECT_GT(withValues.load(), 0) << "The lookups should read values.";
		return waits;
	}
};

TEST_F(IpiConcurrentReadTests, LowMemory_MoreThreadsThanHandles_EveryReadCompletes) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		GTEST_SKIP() << "A single threaded build has no threads to share.";
	}
	if (path.empty()) {
		GTEST_SKIP() << "The ASN data file was not found.";
	}
	EXPECT_GT(readConcurrently(fiftyoneDegreesIpiLowMemoryConfig), 0) <<
		"LowMemory reads everything from the file, so threads should have "
		"waited for the one handle.";
}

TEST_F(IpiConcurrentReadTests, Balanced_MoreThreadsThanHandles_EveryReadCompletes) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		GTEST_SKIP() << "A single threaded build has no threads to share.";
	}
	if (path.empty()) {
		GTEST_SKIP() << "The ASN data file was not found.";
	}
	readConcurrently(fiftyoneDegreesIpiBalancedConfig);
}
