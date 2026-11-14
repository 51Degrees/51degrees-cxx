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

#ifndef FIFTYONE_DEGREES_TEST_SIZE_OTHER_THREAD_HPP
#define FIFTYONE_DEGREES_TEST_SIZE_OTHER_THREAD_HPP

#include "pch.h"
#include <atomic>
#include <cstdio>
#include <functional>
#include <thread>
#include "../../src/common/memory.h"
#include "../../src/common/threading.h"

/**
 * Checks that a size function counts only the memory its own thread
 * allocates. The size is measured once alone, then again while another
 * thread allocates a block during the measurement and holds it until the
 * measurement has ended. Both sizes must be the same, and once the block is
 * freed the tracker must hold no record of it.
 *
 * The other thread is spinning before the measurement starts, and allocates
 * as soon as the allocation functions point at the memory tracker. Its block
 * counts as allocated during the measurement only if the functions still
 * point at the tracker after the allocation, because a measurement points
 * them back before it stops. A short measurement can end before the other
 * thread gets in, so it is repeated until one holds the allocation.
 * @param measure calls the size function and returns the size
 */
inline void expectSizeIgnoresOtherThread(
	const std::function<size_t()>& measure) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		return;
	}
	const size_t blockSize = 1024 * 1024;
	const int maxAttempts = 100;

	// Standard allocation functions until the size function starts, so the
	// other thread can tell when the measurement has begun.
	fiftyoneDegreesUnsetMemoryTracking();
	const size_t alone = measure();
	ASSERT_GT(alone, (size_t)0);

	for (int attempt = 1; attempt <= maxAttempts; attempt++) {
		void* block = nullptr;
		bool during = false;
		std::atomic<bool> spinning(false);
		std::atomic<bool> measured(false);
		std::thread other([&]() {
			decltype(fiftyoneDegreesMalloc) volatile* current =
				&fiftyoneDegreesMalloc;
			spinning = true;
			decltype(fiftyoneDegreesMalloc) allocate = *current;
			while (allocate != fiftyoneDegreesMemoryTrackingMalloc &&
				measured == false) {
				allocate = *current;
			}
			if (allocate == fiftyoneDegreesMemoryTrackingMalloc) {
				block = allocate(blockSize);
				during = *current == fiftyoneDegreesMemoryTrackingMalloc;
			}
		});
		while (spinning == false) {
			std::this_thread::yield();
		}
		const size_t withOther = measure();
		measured = true;
		other.join();

		if (during) {
			printf("The other thread allocated during measurement %d.\n",
				attempt);
			EXPECT_EQ(alone, withOther) <<
				"The size counted memory another thread allocated.";

			// The block was allocated while the size was measured and is
			// freed after it, so the tracker must have no record of it to
			// subtract.
			fiftyoneDegreesMemoryTrackingReset();
			fiftyoneDegreesMemoryTrackingFree(block);
			EXPECT_EQ((size_t)0, fiftyoneDegreesMemoryTrackingGetAllocated())
				<< "The tracker kept a record of memory another thread "
				"allocated while the size was measured.";
			return;
		}

		// The measurement ended before the other thread allocated, or while
		// it did. Free the block either way and measure again.
		if (block != nullptr) {
			fiftyoneDegreesMemoryTrackingFree(block);
		}
		fiftyoneDegreesMemoryTrackingReset();
	}
	FAIL() << "None of " << maxAttempts << " measurements held the other "
		"thread's allocation.";
}

#endif
