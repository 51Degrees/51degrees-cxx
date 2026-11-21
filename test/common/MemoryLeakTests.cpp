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
 
#include "pch.h"
#include <thread>
#include "Base.hpp"
#include "../../src/common/memory.h"
#include "../../src/common/threading.h"

/**
 * Memory Leak test class used to verify that the Base.cpp memory leak check
 * functionality works as expected.
 */
class MemoryLeak : public Base {
protected:

	/**
	 * Calls the base setup method to enable memory leak checking and memory
	 * allocation checking.
	 */
	void SetUp() {
		// Do nothing. The actuall SetUp is called in a test that verify the
		// memory leak checking logic.
	}

	/**
	 * Call the base teardown method to check
	 * for memory leaks and compare expected and actual memory allocations.
	 */
	void TearDown() {
		// Do nothing. The actuall TearDown is called in a test that verify the
		// memory leak checking logic.
	}
};

/**
 * Make sure the memory is allocated but not freed so the memroy leak can be
 * detected. This test follow how Base.cpp SetUp/TearDown process is done by
 * calling Base::SetUpMemoryCheck() at the start and
 * Base:::PerformanceMemoryCheck() at the end.
 */
TEST_F(MemoryLeak, MemoryLeakNotFreed) {
	Base::SetUpMemoryCheck();
	int *leakMem = (int *)fiftyoneDegreesMalloc(sizeof(int));
#ifdef _DEBUG
	// Memory leak feature mainly rely on Debug build so only perform this check
	// in debug mode.
	if (Base::PerformMemoryCheck() == 0) {
		// Free memory here to make sure the test does not actually leak.
		fiftyoneDegreesFree(leakMem);
		FAIL() << "Failed to detect memory leak.";
	}
#endif
	// Free allocated memory after check to make sure the test does not actually leak
	fiftyoneDegreesFree(leakMem);
}

/**
 * Make sure the memory is all freed so that no memory leak is detected.
 * This test follow how Base.cpp SetUp/TearDown process is done by calling
 * Base::SetUpMemoryCheck() at the start and Base:::PerformanceMemoryCheck()
 * at the end.
 */
TEST_F(MemoryLeak, MemoryLeakFreed) {
	Base::SetUpMemoryCheck();
	int *leakMem = (int *)fiftyoneDegreesMalloc(sizeof(int));
	fiftyoneDegreesFree(leakMem);
#ifdef _DEBUG
	// Memory leak feature mainly rely on Debug build so only perform this check
	// in debug mode.
	if (Base::PerformMemoryCheck() != 0) {
		FAIL() << "Failed to detect determine that all memory has been freed.";
	}
#endif
}

/**
 * Test that the tracking free and aligned free give memory the tracker holds
 * no record of to the standard free, uncounted. The size functions point the
 * allocation functions at the tracker while they load a data set, so another
 * thread can free memory it allocated before then through the tracking free.
 */
TEST_F(MemoryLeak, TrackingFree_UntrackedMemory) {
	void *untracked = fiftyoneDegreesMemoryStandardMalloc(sizeof(int));
	void *untrackedAligned = fiftyoneDegreesMemoryStandardMallocAligned(
		16,
		16);
	ASSERT_NE(nullptr, untracked);
	ASSERT_NE(nullptr, untrackedAligned);
	fiftyoneDegreesSetUpMemoryTracking();
	void *tracked = fiftyoneDegreesMalloc(sizeof(int));
	fiftyoneDegreesFree(untracked);
	fiftyoneDegreesFreeAligned(untrackedAligned);
	EXPECT_EQ(sizeof(int), fiftyoneDegreesMemoryTrackingGetAllocated());
	fiftyoneDegreesFree(tracked);
	EXPECT_EQ((size_t)0, fiftyoneDegreesMemoryTrackingGetAllocated());
	fiftyoneDegreesUnsetMemoryTracking();
}

/**
 * Test that while one thread measures, memory another thread allocates is
 * neither counted nor recorded, memory another thread frees is not taken from
 * the count, and every thread is tracked again once the measurement stops.
 */
TEST_F(MemoryLeak, Measuring_CountsOnlyTheMeasuringThread) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		return;
	}

	// A block another thread allocates while every thread is tracked.
	fiftyoneDegreesSetUpMemoryTracking();
	void *before = nullptr;
	std::thread([&]() { before = fiftyoneDegreesMalloc(64); }).join();
	ASSERT_NE(nullptr, before);

	fiftyoneDegreesMemoryTrackingStartMeasuring();
	void *mine = fiftyoneDegreesMalloc(16);
	void *during = nullptr;
	std::thread([&]() {
		during = fiftyoneDegreesMalloc(1024);
		fiftyoneDegreesFree(before);
	}).join();
	ASSERT_NE(nullptr, during);
	EXPECT_EQ((size_t)16, fiftyoneDegreesMemoryTrackingGetAllocated());
	EXPECT_EQ((size_t)16, fiftyoneDegreesMemoryTrackingGetMax());
	fiftyoneDegreesFree(mine);
	EXPECT_EQ((size_t)0, fiftyoneDegreesMemoryTrackingGetAllocated());
	fiftyoneDegreesMemoryTrackingStopMeasuring();
	EXPECT_TRUE(fiftyoneDegreesMalloc == fiftyoneDegreesMemoryStandardMalloc);
	EXPECT_TRUE(fiftyoneDegreesFree == fiftyoneDegreesMemoryStandardFree);

	// The block allocated during the measurement left no record to subtract.
	fiftyoneDegreesMemoryTrackingFree(during);
	EXPECT_EQ((size_t)0, fiftyoneDegreesMemoryTrackingGetAllocated());

	// Once the measurement stops, every thread is tracked again.
	fiftyoneDegreesSetUpMemoryTracking();
	void *after = nullptr;
	std::thread([&]() { after = fiftyoneDegreesMalloc(8); }).join();
	EXPECT_EQ((size_t)8, fiftyoneDegreesMemoryTrackingGetAllocated());
	fiftyoneDegreesFree(after);
	EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
}

/**
 * Test class whose set up method returns before taking the memory checkpoint,
 * as one that finds nothing to test with can.
 */
class MemoryLeakNoCheckpoint : public Base {
protected:
	void SetUp() override {
	}
};

/**
 * Test that a test which never took the checkpoint passes the memory check,
 * rather than being compared with a checkpoint it does not have.
 */
TEST_F(MemoryLeakNoCheckpoint, NoCheckpoint_Passes) {
}

// The googletest of the Visual Studio projects has no skip.
#ifdef GTEST_SKIP_

/**
 * Test class whose tests skip themselves after the memory checkpoint.
 */
class MemoryLeakSkip : public Base {
};

/**
 * Test that a test which skips itself after the checkpoint passes the memory
 * check, where googletest holds its record of the skip on the heap.
 */
TEST_F(MemoryLeakSkip, SkippedAfterCheckpoint_Passes) {
	GTEST_SKIP() << "Skipped so that the memory check must pass the test.";
}

/**
 * Test class that skips each test in its set up method, before the memory
 * checkpoint.
 */
class MemoryLeakSkipInSetUp : public Base {
protected:
	void SetUp() override {
		GTEST_SKIP() << "Skipped before the checkpoint, so that the memory "
			"check must pass the test.";
	}
};

/**
 * Test that a test which skips itself before the checkpoint passes the memory
 * check.
 */
TEST_F(MemoryLeakSkipInSetUp, SkippedBeforeCheckpoint_Passes) {
	FAIL() << "The test ran after its set up method skipped it.";
}

#endif
