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

#ifndef TESTUTILS_SIZEMANAGER_HPP
#define TESTUTILS_SIZEMANAGER_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#define TESTUTILS_NO_INLINE __declspec(noinline)
#else
#define TESTUTILS_NO_INLINE __attribute__((noinline))
#endif

/** How long a size method is given on the small memory the tests offer. */
static const int SizeMethodLimitSeconds = 30;

/** Pointers to write, which is more than the frame of a size method holds. */
static const size_t StackFillPointers = 1024;

/**
 * Ends the process with a failure if this is still in scope after the limit.
 * A call that never returns cannot be stopped from its own thread, and a test
 * program run directly has no other limit, so this stops a hang holding a
 * build.
 */
class TimeLimit {
public:
	explicit TimeLimit(const int seconds)
		: watcher([this, seconds]() {
			std::unique_lock<std::mutex> lock(mutex);
			if (changed.wait_for(
				lock,
				std::chrono::seconds(seconds),
				[this]() { return finished; }) == false) {
				fprintf(
					stderr,
					"Still running after %d seconds, so ending the test.\n",
					seconds);
				fflush(stderr);
				std::_Exit(EXIT_FAILURE);
			}
		}) {
	}

	~TimeLimit() {
		{
			std::lock_guard<std::mutex> lock(mutex);
			finished = true;
		}
		changed.notify_one();
		watcher.join();
	}

private:
	std::mutex mutex;
	std::condition_variable changed;
	bool finished = false;
	std::thread watcher; // Last, as it uses the members above once started.
};

/**
 * Leaves pointers to zeroed memory on the stack below the caller. A manager
 * that a function then declares and does not set reads as one whose active
 * handle has been replaced, and freeing it waits for a new handle for ever.
 * So a test that frees an unset manager hangs every time, rather than
 * hanging, crashing or passing depending on what the stack held.
 */
TESTUTILS_NO_INLINE static void fillStackWithZeroedHandles() {
	static void *zeroed[32] = { nullptr };
	void * volatile filled[StackFillPointers];
	for (size_t i = 0; i < StackFillPointers; i++) {
		filled[i] = zeroed;
	}
}

#endif //TESTUTILS_SIZEMANAGER_HPP
