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
#include "../../src/common/threading.h"
#include "Base.hpp"
#include <thread>
#include <vector>

class Threading : public Base
{
protected:

	void SetUp() {
		Base::SetUp();
	}

	void TearDown() {
		Base::TearDown();
	}
};
/**
 * Swaps a value that matches the comparand, and returns the old value.
 */
TEST_F(Threading, Exchange_Matching) {
    volatile long value = 5;
    long old = FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(value, 6L, 5L);
    ASSERT_EQ(5L, old);
    ASSERT_EQ(6L, (long)value);
}

/**
 * Leaves a value that does not match the comparand, and returns it.
 */
TEST_F(Threading, Exchange_NonMatching) {
    volatile long value = 7;
    long old = FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(value, 6L, 5L);
    ASSERT_EQ(7L, old);
    ASSERT_EQ(7L, (long)value);
}

/**
 * Adds to a plain counter from several threads, each taking the mutex for
 * every addition. A mutex that let two threads in at once would lose additions.
 */
TEST_F(Threading, Mutex_OneThreadAtATime) {
    static const int threadCount = 8;
    static const int additionsPerThread = 100000;

    fiftyoneDegreesMutex mutex;
    FIFTYONE_DEGREES_MUTEX_CREATE(mutex);

    int total = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back([&mutex, &total]() {
            for (int j = 0; j < additionsPerThread; j++) {
                FIFTYONE_DEGREES_MUTEX_LOCK(&mutex);
                total++;
                FIFTYONE_DEGREES_MUTEX_UNLOCK(&mutex);
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
    FIFTYONE_DEGREES_MUTEX_CLOSE(mutex);

    ASSERT_EQ(threadCount * additionsPerThread, total);
}

/**
 * Checks the processor count is at least one and no more than a pool can
 * hold, and is one when the library is single threaded.
 */
TEST_F(Threading, ProcessorCount) {
    const uint16_t count = fiftyoneDegreesThreadingGetProcessorCount();
    EXPECT_GE(count, 1);
    EXPECT_LE(count, FIFTYONE_DEGREES_PROCESSOR_COUNT_MAX);
    if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
        EXPECT_EQ(1, count);
    }
}
