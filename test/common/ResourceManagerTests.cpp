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
#include "../../src/common/resource.h"
#include "Base.hpp"
#include <chrono>
#include <thread>

class ResourceManager : public Base
{
private:

	/**
	* Set the resource to true to indicate this method has been called by the
	* resource manager.
	*/
	static void freeResource(void *resourcePtr) {
		bool *resource = (bool*)resourcePtr;
		*resource = true;
	}

protected:

	/**
	 * Initialise the resource manager with a boolean resource, and set the
	 * resource to false to indicate the free method has not yet been called.
	 */
	void SetUp() {
		Base::SetUp();
		resource = false;
		fiftyoneDegreesResourceManagerInit(
			&manager,
			&resource,
			&resourceHandle,
			ResourceManager::freeResource);
	}

	/**
	 * Releases the resource manager structure if one was created. Then calls
	 * the base teardown method to check for memory leaks and compare expected
	 * and actual memory allocations.
	 */
	void TearDown() {
		disposeManager();
		ASSERT_TRUE(resource) <<
			"The resource was not closed.";
		Base::TearDown();
	}

	/**
	 * Dispose of the resource manager and check that the resource has been 
	 * freed.
	 */
	void disposeManager() {
		if (disposed == false) {
			disposed = true;
			fiftyoneDegreesResourceManagerFree(&manager);
		}
	}

	fiftyoneDegreesResourceManager manager;

	bool disposed = false;

	// The resource to use with the test.
	bool resource;

	// The handle assigned to the resource.
	fiftyoneDegreesResourceHandle *resourceHandle;
};

/**
 * Check that a resource manager contains the correct resource when it is
 * initialised.
 */
TEST_F(ResourceManager, Init) {
	ASSERT_EQ((void*)&resource, (void*)manager.active->resource) <<
		"The resource was not set correctly.";
}

/**
 * Check that the IncUse method returns a valid handle to the resource.
 */
TEST_F(ResourceManager, Handle_Inc) {
	fiftyoneDegreesResourceHandle *handle =
		fiftyoneDegreesResourceHandleIncUse(&manager);
	ASSERT_NE((void*)NULL, (void*)handle) <<
		"A null handle was returned.";
	ASSERT_EQ((void*)&resource, (void*)handle->resource) <<
		"The handle does not contain the correct resource.";
	ASSERT_EQ(1, fiftyoneDegreesResourceHandleGetUse(handle)) <<
		"The in use counter was not incremented correctly.";
	ASSERT_EQ(&manager, handle->manager) <<
		"The handle is not linked to the manager.";
	fiftyoneDegreesResourceHandleDecUse(handle);
}

/**
 * Check that the DecUse method decrements the in use counter.
 */
TEST_F(ResourceManager, Handle_IncDec) {
	fiftyoneDegreesResourceHandle *handle =
		fiftyoneDegreesResourceHandleIncUse(&manager);
	ASSERT_EQ(1, fiftyoneDegreesResourceHandleGetUse(handle)) <<
		"The in use counter was not incremented correctly.";
	fiftyoneDegreesResourceHandleDecUse(handle);
	ASSERT_EQ(0, fiftyoneDegreesResourceHandleGetUse(handle)) <<
		"The in use counter was not decremented correctly.";
}

/**
 * Check that the resource replace method correctly replaces the resource, and
 * that all resources are freed.
 */
TEST_F(ResourceManager, ResourceReplace) {
	fiftyoneDegreesResourceHandle *handle = NULL;
	bool newResource = false;
	fiftyoneDegreesResourceReplace(
		&manager,
		(void*)&newResource,
		&handle);
	ASSERT_NE((void*)NULL, (void*)handle) <<
		"The resource was not replaced in the manager.";
	ASSERT_EQ(&newResource, handle->resource) <<
		"The new resource is not correct.";
	disposeManager();
	ASSERT_TRUE(newResource) <<
		"The new resource was not closed.";
}

/**
 * Check that the manager free method leaves the resource open until it is
 * released, and that the resource is freed in the end.
 */
TEST_F(ResourceManager, Free_HandleInUse) {
	fiftyoneDegreesResourceHandle *handle;
	handle = fiftyoneDegreesResourceHandleIncUse(&manager);
	disposeManager();
	ASSERT_EQ(&resource, handle->resource) <<
		"The new resource is not correct.";
	ASSERT_FALSE(resource) <<
		"The old resource was closed prematurely.";
	fiftyoneDegreesResourceHandleDecUse(handle);
	ASSERT_TRUE(resource) <<
		"The new resource was not closed.";
}

/**
 * Check that the resource replace method leaves the old resource open until it
 * is released.
 */
TEST_F(ResourceManager, ResourceReplace_HandleInUse) {
	fiftyoneDegreesResourceHandle *oldHandle;
	fiftyoneDegreesResourceHandle *newHandle;
	bool newResource = false;
	oldHandle = fiftyoneDegreesResourceHandleIncUse(&manager);
	fiftyoneDegreesResourceReplace(&manager, (void*)&newResource, &newHandle);
	ASSERT_NE((void*)NULL, (void*)newHandle) <<
		"The resource was not replaced in the manager.";
	ASSERT_EQ(&newResource, newHandle->resource) <<
		"The new resource is not correct.";
	ASSERT_FALSE(resource) <<
		"The old resource was closed prematurely.";
	fiftyoneDegreesResourceHandleDecUse(oldHandle);
	disposeManager();
	ASSERT_TRUE(newResource) <<
		"The new resource was not closed.";
}

// Number of threads
#define THREAD_COUNT 8
// Number of Inc/Dec per thread
#define NUMBER_OF_UPDATES 100000
// Number of reloads
#define NUMBER_OF_RELOADS 4

/*
 * The return type of each thread routine below, and the value it returns, so
 * that every routine has the type FIFTYONE_DEGREES_THREAD_CREATE takes and is
 * passed without a cast. On Windows that includes the calling convention
 * CreateThread uses.
 */
#ifdef _MSC_VER
#define RESOURCE_THREAD_RESULT DWORD WINAPI
#define RESOURCE_THREAD_DONE 0
#else
#define RESOURCE_THREAD_RESULT void*
#define RESOURCE_THREAD_DONE nullptr
#endif

/*
 * Reads a flag that another thread sets at the same time, as one atomic load,
 * so the thread sees it change and the read is ordered after the set. A flag
 * is set with FIFTYONE_DEGREES_INTERLOCK_INC.
 */
static long readFlag(volatile long *flag) {
#ifdef _MSC_VER
	return *flag;
#else
	return __atomic_load_n(flag, __ATOMIC_SEQ_CST);
#endif
}

/*
 * Run by each thread to increase resource 
 * usage counter
 */
static RESOURCE_THREAD_RESULT runResourceInc(void *state) {
	fiftyoneDegreesResourceManager *manager =
		(fiftyoneDegreesResourceManager *)state;
	for (int i = 0; i < NUMBER_OF_UPDATES; i++) {
		fiftyoneDegreesResourceHandleIncUse(manager);
	}
	return RESOURCE_THREAD_DONE;
}

/*
 * Run by each thread to decrease resource 
 * usage counter
 */
static RESOURCE_THREAD_RESULT runResourceDec(void *state) {
	fiftyoneDegreesResourceHandle *handle =
		(fiftyoneDegreesResourceHandle *)state;
	for (int i = 0; i < NUMBER_OF_UPDATES; i++) {
		fiftyoneDegreesResourceHandleDecUse(handle);
	}
	return RESOURCE_THREAD_DONE;
}

/*
 * Start threads
 */
static void startThreads(
	FIFTYONE_DEGREES_THREAD *threads, 
	FIFTYONE_DEGREES_THREAD_ROUTINE threadRoutine,
	void *state) {
	for (int i = 0; i < THREAD_COUNT; i++) {
		FIFTYONE_DEGREES_THREAD_CREATE(
			threads[i],
			threadRoutine,
			state);
	}
}

/*
 * Wait and join threads
 */
static void joinThreads(FIFTYONE_DEGREES_THREAD *threads) {
	for (int i = 0; i < THREAD_COUNT; i++) {
		FIFTYONE_DEGREES_THREAD_JOIN(threads[i]);
		FIFTYONE_DEGREES_THREAD_CLOSE(threads[i]);
	}
}

/*
 * Check that the resource usage counter is as expected
 * after being modified in a multi-threading environment
 */
TEST_F(ResourceManager, MultiThreading_HandleIncDec) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];

		// Run an initial Inc/Dec
		fiftyoneDegreesResourceHandle *handle =
			fiftyoneDegreesResourceHandleIncUse(&manager);
		ASSERT_EQ(1, fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not incremented correctly.";
		fiftyoneDegreesResourceHandleDecUse(handle);
		ASSERT_EQ(0, fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not decremented correctly.";

		// Start threads which increase the usage counter
		startThreads(
			threads,
			&runResourceInc,
			&manager);
		joinThreads(threads);
		ASSERT_EQ(
			THREAD_COUNT * NUMBER_OF_UPDATES, 
			fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not increased correctly.";

		// Start threads which decrease the usage counter
		startThreads(
			threads,
			&runResourceDec,
			handle);
		joinThreads(threads);
		ASSERT_EQ(
			0, fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not decreased correctly.";
	}
}

/*
 * Check that the total usage count of all resouce handles
 * is equal to the total number of increases done by all
 * threads while resource under management is being reloaded.
 * 
 * NOTE: There is no tests for reloads during decrease of usage
 * counts under multi-threads environment. The reason for that 
 * is because the decrease is done directly to the handle and 
 * reload does not impact the handle once it has been replaced.
 */
TEST_F(ResourceManager, MultiThreading_HandleReplace_Inc) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];
		uint32_t i;

		// Run an initual Inc/Dec
		fiftyoneDegreesResourceHandle *handle =
			fiftyoneDegreesResourceHandleIncUse(&manager);
		ASSERT_EQ(1, fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not incremented correctly.";
		fiftyoneDegreesResourceHandleDecUse(handle);
		ASSERT_EQ(0, fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use counter was not decremented correctly.";

		// Start threads which increase the usage counter
		startThreads(
			threads,
			&runResourceInc,
			&manager);

		// Perform reloads
		fiftyoneDegreesResourceHandle *newHandles[NUMBER_OF_RELOADS];
		bool newResources[NUMBER_OF_RELOADS];
		for (i = 0; i < NUMBER_OF_RELOADS; i++) {
			// Pause between each load for threads
			// to have time updating the new handle
#ifdef _MSC_VER
			Sleep(20); // milliseconds
#else
			usleep(20000); // microseconds
#endif
			newHandles[i] = NULL;
			newResources[i] = false;
			fiftyoneDegreesResourceReplace(
				&manager,
				(void*)&newResources[i],
				&newHandles[i]);
		}

		// Wait for threads to finish
		joinThreads(threads);

		// Check that total usage count of all handles is the same
		// as number of increases performed by all threads
		uint32_t total = 0;
		if (resource == false) {
			total += fiftyoneDegreesResourceHandleGetUse(handle);
		}

		for (i = 0; i < NUMBER_OF_RELOADS; i++) {
			if (newResources[i] == false) {
				total += fiftyoneDegreesResourceHandleGetUse(newHandles[i]);
			}
		}
		ASSERT_EQ((uint32_t)(THREAD_COUNT * NUMBER_OF_UPDATES), total) <<
			"The in use counter was not increased correctly.";

		// Decrease usage counter of all handles to 0 to mark
		// as no longer used
		uint32_t usageCount = 0;
		if (resource == false) {
			usageCount = fiftyoneDegreesResourceHandleGetUse(handle);
			for (i = 0; i < usageCount; i++) {
				fiftyoneDegreesResourceHandleDecUse(handle);
			}
		}

		for (i = 0; i < NUMBER_OF_RELOADS; i++) {
			if (newResources[i] == false) {
				usageCount = fiftyoneDegreesResourceHandleGetUse(newHandles[i]);
				for (uint32_t j = 0; j < usageCount; j++) {
					fiftyoneDegreesResourceHandleDecUse(newHandles[i]);
				}
			}
		}
		// Manager has to be disposed here because resources
		// are defined locally.
		disposeManager();
	}
}

// Number of times the resource is replaced while threads are using it
#define NUMBER_OF_REPLACEMENTS 20000

/*
 * A resource which can tell a thread that it has been freed.
 */
typedef struct checked_resource_t {
	fiftyoneDegreesResourceHandle *handle;
	volatile bool live;
} CheckedResource;

/*
 * The state shared by the threads using a resource that is being replaced.
 */
typedef struct replaced_state_t {
	fiftyoneDegreesResourceManager *manager;
	volatile long finished;
	volatile long faults;
} ReplacedState;

static CheckedResource* newCheckedResource() {
	CheckedResource *resource = new CheckedResource();
	resource->handle = nullptr;
	resource->live = true;
	return resource;
}

static void freeCheckedResource(void *pointer) {
	CheckedResource *resource = (CheckedResource*)pointer;
	resource->live = false;
	delete resource;
}

/*
 * Run by each thread to take and release handles, counting any that do not
 * lead to a live resource which has that handle.
 */
static RESOURCE_THREAD_RESULT runResourceIncDec(void *pointer) {
	ReplacedState *state = (ReplacedState*)pointer;
	while (readFlag(&state->finished) == 0) {
		fiftyoneDegreesResourceHandle *handle =
			fiftyoneDegreesResourceHandleIncUse(state->manager);
		if (handle == nullptr) {
			FIFTYONE_DEGREES_INTERLOCK_INC(&state->faults);
			continue;
		}
		const CheckedResource *resource =
			(const CheckedResource*)handle->resource;
		if (resource == nullptr ||
			resource->live == false ||
			resource->handle != handle) {
			FIFTYONE_DEGREES_INTERLOCK_INC(&state->faults);
		}
		fiftyoneDegreesResourceHandleDecUse(handle);
	}
	return RESOURCE_THREAD_DONE;
}

/*
 * Check that threads taking and releasing handles while the resource is
 * replaced always get a live resource, and that no memory is read after it
 * has been freed when run with AddressSanitizer. The threads spend most of
 * their time between reading the active handle and updating its counter,
 * which is where another thread can release the handle's last reference.
 */
TEST_F(ResourceManager, MultiThreading_HandleReplace_IncDec) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];
		fiftyoneDegreesResourceManager replacedManager;
		ReplacedState state = { &replacedManager, 0, 0 };
		CheckedResource *first = newCheckedResource();
		fiftyoneDegreesResourceManagerInit(
			&replacedManager,
			first,
			&first->handle,
			freeCheckedResource);

		startThreads(
			threads,
			&runResourceIncDec,
			&state);
		for (int i = 0; i < NUMBER_OF_REPLACEMENTS; i++) {
			CheckedResource *next = newCheckedResource();
			fiftyoneDegreesResourceReplace(
				&replacedManager,
				next,
				&next->handle);
		}
		FIFTYONE_DEGREES_INTERLOCK_INC(&state.finished);
		joinThreads(threads);

		fiftyoneDegreesResourceManagerFree(&replacedManager);
		ASSERT_EQ(0L, (long)state.faults) <<
			"A thread was given a handle without a live resource.";
	}
}

// Number of times each thread replaces the resource while the others do too
#define CONCURRENT_REPLACEMENTS 5000

// Number of resources freed by freeCountedResource
static volatile long countedFrees = 0;

// Number of replacing threads that have started, so that none starts
// replacing until all of them are ready
static volatile long readyThreads = 0;

static void freeCountedResource(void *pointer) {
	FIFTYONE_DEGREES_INTERLOCK_INC(&countedFrees);
	delete (CheckedResource*)pointer;
}

/*
 * Run by each thread to replace the resource many times. Each thread waits
 * until every thread has started, so that the replacements overlap.
 */
static RESOURCE_THREAD_RESULT runResourceReplace(void *pointer) {
	fiftyoneDegreesResourceManager *manager =
		(fiftyoneDegreesResourceManager*)pointer;
	FIFTYONE_DEGREES_INTERLOCK_INC(&readyThreads);
	while (readFlag(&readyThreads) < THREAD_COUNT) {
		std::this_thread::yield();
	}
	for (int i = 0; i < CONCURRENT_REPLACEMENTS; i++) {
		CheckedResource *next = newCheckedResource();
		fiftyoneDegreesResourceReplace(manager, next, &next->handle);
	}
	return RESOURCE_THREAD_DONE;
}

/*
 * Check that when several threads replace the resource at the same time,
 * every resource put in place is freed in the end. A replacement that loses
 * the race to another must try again rather than leave its resource
 * neither active nor freed.
 */
TEST_F(ResourceManager, MultiThreading_ResourceReplace_Concurrent) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];
		fiftyoneDegreesResourceManager replacedManager;
		countedFrees = 0;
		readyThreads = 0;
		CheckedResource *first = newCheckedResource();
		fiftyoneDegreesResourceManagerInit(
			&replacedManager,
			first,
			&first->handle,
			freeCountedResource);

		startThreads(
			threads,
			&runResourceReplace,
			&replacedManager);
		joinThreads(threads);

		fiftyoneDegreesResourceManagerFree(&replacedManager);
		ASSERT_EQ(
			(long)THREAD_COUNT * CONCURRENT_REPLACEMENTS + 1,
			(long)countedFrees) <<
			"A resource put in place while another thread replaced the " <<
			"resource was never freed.";
	}
}

/*
 * Check that each replaced resource is freed once, and only once, while
 * threads take and release handles. A thread that read the active handle
 * just before it was replaced can take and release it after its count has
 * reached 0, so two threads can each try to free it, and only one may.
 */
TEST_F(ResourceManager, MultiThreading_HandleReplace_FreesOnce) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];
		fiftyoneDegreesResourceManager replacedManager;
		ReplacedState state = { &replacedManager, 0, 0 };
		countedFrees = 0;
		CheckedResource *first = newCheckedResource();
		fiftyoneDegreesResourceManagerInit(
			&replacedManager,
			first,
			&first->handle,
			freeCountedResource);

		startThreads(
			threads,
			&runResourceIncDec,
			&state);
		for (int i = 0; i < NUMBER_OF_REPLACEMENTS; i++) {
			CheckedResource *next = newCheckedResource();
			fiftyoneDegreesResourceReplace(
				&replacedManager,
				next,
				&next->handle);
		}
		FIFTYONE_DEGREES_INTERLOCK_INC(&state.finished);
		joinThreads(threads);
		EXPECT_EQ(0L, (long)state.faults) <<
			"A thread was given a handle without a live resource.";
		EXPECT_EQ((long)NUMBER_OF_REPLACEMENTS, (long)countedFrees) <<
			"The replaced resources were not each freed once after the " <<
			"threads released them.";

		fiftyoneDegreesResourceManagerFree(&replacedManager);
		ASSERT_EQ((long)NUMBER_OF_REPLACEMENTS + 1, (long)countedFrees) <<
			"The active resource was not freed with the manager.";
	}
}

/*
 * A thread that takes a handle which already has the most users it can have.
 */
typedef struct waiting_state_t {
	fiftyoneDegreesResourceManager *manager;
	fiftyoneDegreesResourceHandle *handle;
	volatile long taken;
} WaitingState;

static RESOURCE_THREAD_RESULT runResourceIncAtMax(void *pointer) {
	WaitingState *state = (WaitingState*)pointer;
	state->handle = fiftyoneDegreesResourceHandleIncUse(state->manager);
	FIFTYONE_DEGREES_INTERLOCK_INC(&state->taken);
	return RESOURCE_THREAD_DONE;
}

/*
 * Check that a thread taking a handle with the most users the count can hold
 * waits until another user releases it, rather than taking the count past
 * its bits and into the claimed bit.
 */
TEST_F(ResourceManager, MultiThreading_HandleInc_WaitsAtMax) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		fiftyoneDegreesResourceHandle *handle =
			fiftyoneDegreesResourceHandleIncUse(&manager);

		// Stand in for the most users the handle can have.
		handle->counter = FIFTYONE_DEGREES_RESOURCE_IN_USE_MAX;
		WaitingState state = { &manager, nullptr, 0 };
		FIFTYONE_DEGREES_THREAD thread;
		FIFTYONE_DEGREES_THREAD_CREATE(
			thread,
			&runResourceIncAtMax,
			&state);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		EXPECT_EQ(0L, (long)state.taken) <<
			"A handle was taken past the most users it can have.";
		EXPECT_EQ(
			(long)FIFTYONE_DEGREES_RESOURCE_IN_USE_MAX,
			(long)fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use count went past the most users it can have.";

		// One user releasing the handle lets the waiting thread take it.
		fiftyoneDegreesResourceHandleDecUse(handle);
		FIFTYONE_DEGREES_THREAD_JOIN(thread);
		FIFTYONE_DEGREES_THREAD_CLOSE(thread);
		EXPECT_EQ(1L, (long)state.taken) <<
			"The waiting thread did not take the handle once it was released.";
		EXPECT_EQ(handle, state.handle) <<
			"The waiting thread took a different handle.";
		EXPECT_EQ(
			(long)FIFTYONE_DEGREES_RESOURCE_IN_USE_MAX,
			(long)fiftyoneDegreesResourceHandleGetUse(handle)) <<
			"The in use count is wrong after the waiting thread took it.";

		// Put back the one real user, and release it.
		handle->counter = 1;
		fiftyoneDegreesResourceHandleDecUse(handle);
	}
}

// Number of times the threads race to release the last uses of a handle
#define RELEASE_RACES 500

/*
 * The handle the threads release, and what lets them start together.
 */
typedef struct release_race_t {
	fiftyoneDegreesResourceHandle *handle;
	volatile long ready;
	volatile long go;
} ReleaseRace;

/*
 * Run by each thread to release its use of a replaced handle as soon as
 * every thread is ready, so that the releases overlap.
 */
static RESOURCE_THREAD_RESULT runResourceRelease(void *pointer) {
	ReleaseRace *race = (ReleaseRace*)pointer;
	FIFTYONE_DEGREES_INTERLOCK_INC(&race->ready);
	while (readFlag(&race->go) == 0) {
		std::this_thread::yield();
	}
	fiftyoneDegreesResourceHandleDecUse(race->handle);
	return RESOURCE_THREAD_DONE;
}

/*
 * Check that when several threads release the last uses of a replaced handle
 * at the same time, its resource is freed once, and not while any of them
 * still holds it.
 */
TEST_F(ResourceManager, MultiThreading_FinalRelease_FreesOnce) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe()) {
		FIFTYONE_DEGREES_THREAD threads[THREAD_COUNT];
		for (int i = 0; i < RELEASE_RACES; i++) {
			fiftyoneDegreesResourceManager raceManager;
			countedFrees = 0;
			CheckedResource *first = newCheckedResource();
			fiftyoneDegreesResourceManagerInit(
				&raceManager,
				first,
				&first->handle,
				freeCountedResource);
			ReleaseRace race = { nullptr, 0, 0 };
			for (int t = 0; t < THREAD_COUNT; t++) {
				race.handle = fiftyoneDegreesResourceHandleIncUse(&raceManager);
			}
			CheckedResource *next = newCheckedResource();
			fiftyoneDegreesResourceReplace(&raceManager, next, &next->handle);
			ASSERT_EQ(0L, (long)countedFrees) <<
				"A replaced resource was freed while it was in use.";

			startThreads(
				threads,
				&runResourceRelease,
				&race);
			while (readFlag(&race.ready) < THREAD_COUNT) {
				std::this_thread::yield();
			}
			FIFTYONE_DEGREES_INTERLOCK_INC(&race.go);
			joinThreads(threads);
			ASSERT_EQ(1L, (long)countedFrees) <<
				"The replaced resource was not freed exactly once when its " <<
				"last uses were released together, on race " << i << ".";

			fiftyoneDegreesResourceManagerFree(&raceManager);
			ASSERT_EQ(2L, (long)countedFrees) <<
				"The active resource was not freed with the manager.";
		}
	}
}
