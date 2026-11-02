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
#include "Base.hpp"
#include "../../src/common/memory.h"

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
