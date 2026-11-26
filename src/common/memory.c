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

#include "memory.h"
#include "fiftyone.h"

#ifndef FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS
#ifndef FIFTYONE_DEGREES_NO_THREADING
#define FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS 20
#else
#define FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS 1
#endif
#else 
#endif

static int shardDivider = sizeof(void*) * 3;

/*
 * The tracker's state is set up once, by the first thread to need it, and is
 * never torn down, because a thread that read a tracking function before the
 * allocation functions were pointed back can still call it at any time.
 */
#define SETUP_NONE 0 /* Not set up */
#define SETUP_RUNNING 1 /* One thread is setting up */
#define SETUP_DONE 2 /* Set up, and stays so */

#ifndef FIFTYONE_DEGREES_NO_THREADING

/*
 * Threads read the set up flag while another thread sets it, so every access
 * is atomic. Reading SETUP_DONE also makes the set up visible, as acquire
 * order there pairs with the interlocked exchange that stores it, which is a
 * full barrier. Visual Studio reads with an interlocked exchange that changes
 * nothing, which is a full barrier on every processor.
 */
static long setUp = SETUP_NONE;
#ifdef _MSC_VER
#define SETUP_READ() INTERLOCK_EXCHANGE(setUp, SETUP_NONE, SETUP_NONE)
#else
#define SETUP_READ() __atomic_load_n(&setUp, __ATOMIC_ACQUIRE)
#endif
#define SETUP_CLAIM() \
	(INTERLOCK_EXCHANGE(setUp, SETUP_RUNNING, SETUP_NONE) == SETUP_NONE)
#define SETUP_FINISH() INTERLOCK_EXCHANGE(setUp, SETUP_DONE, SETUP_RUNNING)

/*
 * Stores an allocation function pointer in one atomic access with release
 * order, so a thread that reads the new pointer with
 * FIFTYONE_DEGREES_ALLOCATOR_READ also sees what this thread did before,
 * such as starting a measurement. Visual Studio stores an aligned pointer
 * sized volatile in one access, after a full barrier.
 */
#ifdef _MSC_VER
#define ALLOCATOR_WRITE(v, t, n) \
	(MemoryBarrier(), *(t volatile*)&(v) = (n))
#else
#define ALLOCATOR_WRITE(v, t, n) __atomic_store_n(&(v), (n), __ATOMIC_RELEASE)
#endif

#define STATE_LOCK() FIFTYONE_DEGREES_MUTEX_LOCK(&state.lock)
#define STATE_UNLOCK() FIFTYONE_DEGREES_MUTEX_UNLOCK(&state.lock)
#define SHARD_LOCK(s) FIFTYONE_DEGREES_MUTEX_LOCK(&state.locks[s])
#define SHARD_UNLOCK(s) FIFTYONE_DEGREES_MUTEX_UNLOCK(&state.locks[s])

#else

static long setUp = SETUP_NONE;
#define SETUP_READ() setUp
#define SETUP_CLAIM() (setUp == SETUP_NONE ? (setUp = SETUP_RUNNING, true) : false)
#define SETUP_FINISH() (setUp = SETUP_DONE)
#define ALLOCATOR_WRITE(v, t, n) ((v) = (n))
#define STATE_LOCK()
#define STATE_UNLOCK()
#define SHARD_LOCK(s)
#define SHARD_UNLOCK(s)

#endif

#define WRITE_TRACKING_FUNCTIONS() \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesMalloc, \
		fiftyoneDegreesMallocFunction, \
		fiftyoneDegreesMemoryTrackingMalloc); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesMallocAligned, \
		fiftyoneDegreesMallocAlignedFunction, \
		fiftyoneDegreesMemoryTrackingMallocAligned); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesFree, \
		fiftyoneDegreesFreeFunction, \
		fiftyoneDegreesMemoryTrackingFree); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesFreeAligned, \
		fiftyoneDegreesFreeFunction, \
		fiftyoneDegreesMemoryTrackingFreeAligned)

#define WRITE_STANDARD_FUNCTIONS() \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesMalloc, \
		fiftyoneDegreesMallocFunction, \
		fiftyoneDegreesMemoryStandardMalloc); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesMallocAligned, \
		fiftyoneDegreesMallocAlignedFunction, \
		fiftyoneDegreesMemoryStandardMallocAligned); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesFree, \
		fiftyoneDegreesFreeFunction, \
		fiftyoneDegreesMemoryStandardFree); \
	ALLOCATOR_WRITE( \
		fiftyoneDegreesFreeAligned, \
		fiftyoneDegreesFreeFunction, \
		fiftyoneDegreesMemoryStandardFreeAligned)

typedef struct memory_allocation_t {
	fiftyoneDegreesTreeNode tree; /* Tree node data structure */
	size_t size; /* The amount of memory allocated at pointer */
} allocation;

typedef struct memory_allocation_tree_t {
	fiftyoneDegreesTreeRoot roots[FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS];
#ifndef FIFTYONE_DEGREES_NO_THREADING
	fiftyoneDegreesMutex locks[FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS];
	fiftyoneDegreesMutex lock;
	bool measuring; /* True while one thread measures what it allocates */
	FIFTYONE_DEGREES_THREAD_ID measurer; /* The thread that measures */
#endif
	size_t allocated;
	size_t max;
} allocationTree;

static allocationTree state;

bool fiftyoneDegreesMemoryAdvance(
	fiftyoneDegreesMemoryReader *reader,
	size_t advanceBy) {
	if (reader == NULL || reader->current == NULL) {
		return false;
	}
	reader->current += advanceBy;
	if (reader->current > reader->lastByte) {
		return false;
	}
	return true;
}

void* fiftyoneDegreesMemoryStandardMalloc(size_t size) {
	return malloc(size);
}

void* fiftyoneDegreesMemoryStandardMallocAligned(int alignment, size_t size) {
	size += size % alignment;
#if defined( _MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	return _aligned_malloc(size, alignment);
#elif defined (__APPLE__)
	void *pointer;
	if (posix_memalign(&pointer, alignment, size) == 0) {
		return pointer;
	}
	else {
		return NULL;
	}

#else
	return aligned_alloc(alignment, size);
#endif
}

static int getShardFromPointer(void *pointer) {
	// Cast through uintptr_t, which is the size of a pointer on every
	// platform, before widening to 64 bits.
	return (((uint64_t)(uintptr_t)pointer) / shardDivider) %
		FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS;
}

/**
 * Creates the tracker's locks and trees the first time any thread needs
 * them. A thread that finds another setting up waits for it, which takes
 * only as long as creating the locks.
 */
static void ensureSetUp() {
	int i;
	if (SETUP_READ() == SETUP_DONE) {
		return;
	}
	if (SETUP_CLAIM()) {
		for (i = 0; i < FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS; i++) {
#ifndef FIFTYONE_DEGREES_NO_THREADING
			FIFTYONE_DEGREES_MUTEX_CREATE(state.locks[i]);
#endif
			fiftyoneDegreesTreeRootInit(&state.roots[i]);
		}
#ifndef FIFTYONE_DEGREES_NO_THREADING
		FIFTYONE_DEGREES_MUTEX_CREATE(state.lock);
#endif
		SETUP_FINISH();
		return;
	}
	while (SETUP_READ() != SETUP_DONE) {
	}
}

/**
 * True while a thread other than the calling one measures, so that the
 * calling thread's memory is left out of the measurement. The caller holds
 * the state lock.
 */
static bool isOtherThreadMeasuring() {
#ifndef FIFTYONE_DEGREES_NO_THREADING
	return state.measuring &&
		FIFTYONE_DEGREES_THREAD_ID_EQUAL(
			state.measurer,
			FIFTYONE_DEGREES_THREAD_ID_CURRENT()) == false;
#else
	return false;
#endif
}

static void trackAllocation(void* pointer, size_t size) {
	allocation *record, *stale;
	int shard;

	if (pointer == NULL) {
		return;
	}

	// Check and count under one lock, so a measurement can't start or stop
	// between them. Memory allocated while another thread measures is not
	// recorded, so its free is not counted either.
	STATE_LOCK();
	if (isOtherThreadMeasuring()) {
		STATE_UNLOCK();
		return;
	}
	state.allocated += size;
	if (state.allocated > state.max) {
		state.max = state.allocated;
	}
	STATE_UNLOCK();

	// Create a new tree node to record the allocation.
	record = (allocation*)malloc(sizeof(allocation));
	shard = getShardFromPointer(pointer);
	assert(record != NULL);
	assert(shard < FIFTYONE_DEGREES_MEMORY_TRACKER_SHARDS);
	fiftyoneDegreesTreeNodeInit(&record->tree, &state.roots[shard]);
	record->tree.key = (int64_t)(intptr_t)pointer;
	record->size = size;

	// A record for the same address is left from memory the standard free
	// released after tracking ended, as the address is live again. It is
	// replaced, as the tree holds one record for each address.
	SHARD_LOCK(shard);
	stale = (allocation*)fiftyoneDegreesTreeFind(
		&state.roots[shard],
		record->tree.key);
	if (stale != NULL) {
		fiftyoneDegreesTreeDelete(&stale->tree);
	}
	fiftyoneDegreesTreeInsert(&record->tree);
	SHARD_UNLOCK(shard);
	free(stale);
}

static void untrackAllocation(void *pointer) {
	size_t size;
	int shard;
	allocation* record;

	// The standard free accepts a null pointer, and the API relies on that
	// throughout its clean up paths, so the tracking free must accept one
	// too rather than assert on a record it will never find.
	if (pointer == NULL) {
		return;
	}
	ensureSetUp();
	shard = getShardFromPointer(pointer);

	// Get the size of the memory being freed and free the tracking memory.
	SHARD_LOCK(shard);
	record = (allocation*)fiftyoneDegreesTreeFind(
		&state.roots[shard],
		(int64_t)(intptr_t)pointer);
	// Memory the tracker did not allocate has no record, as when another
	// thread frees memory it allocated before the size functions pointed the
	// allocation functions here. It is freed without being counted.
	if (record == NULL) {
		SHARD_UNLOCK(shard);
		return;
	}
	size = record->size;
	fiftyoneDegreesTreeDelete(&record->tree);
	SHARD_UNLOCK(shard);
	free(record);

	// Memory another thread frees while one thread measures was allocated
	// before the measurement started, so it is not taken from the count.
	STATE_LOCK();
	if (isOtherThreadMeasuring() == false) {
		state.allocated -= size;
	}
	STATE_UNLOCK();
}

void* fiftyoneDegreesMemoryTrackingMallocAligned(
	int alignment,
	size_t size) {
	void* pointer;
	ensureSetUp();
	pointer = MemoryStandardMallocAligned(alignment, size);
	trackAllocation(pointer, size + size % alignment);
	return pointer;
}

void* fiftyoneDegreesMemoryTrackingMalloc(size_t size) {
	void* pointer;
	ensureSetUp();
	pointer = MemoryStandardMalloc(size);
	trackAllocation(pointer, size);
	return pointer;
}

void fiftyoneDegreesMemoryStandardFree(void *pointer) {
	free(pointer);
}

void fiftyoneDegreesMemoryStandardFreeAligned(void* pointer) {
#if defined( _MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	_aligned_free(pointer);
#else
	free(pointer);
#endif
}

void fiftyoneDegreesMemoryTrackingFree(void *pointer) {
	// The record goes before the memory, as another thread can be given the
	// same address once it is freed.
	untrackAllocation(pointer);
	MemoryStandardFree(pointer);
}

void fiftyoneDegreesMemoryTrackingFreeAligned(void* pointer) {
	untrackAllocation(pointer);
	MemoryStandardFreeAligned(pointer);
}

size_t fiftyoneDegreesMemoryTrackingGetMax() {
	size_t max;
	ensureSetUp();
	STATE_LOCK();
	max = state.max;
	STATE_UNLOCK();
	return max;
}

size_t fiftyoneDegreesMemoryTrackingGetAllocated() {
	size_t allocated;
	ensureSetUp();
	STATE_LOCK();
	allocated = state.allocated;
	STATE_UNLOCK();
	return allocated;
}

void fiftyoneDegreesMemoryTrackingReset() {
	ensureSetUp();
	STATE_LOCK();
	state.allocated = 0;
	state.max = 0;
	STATE_UNLOCK();
}

void fiftyoneDegreesSetUpMemoryTracking() {
	fiftyoneDegreesMemoryTrackingReset();
	WRITE_TRACKING_FUNCTIONS();
}

size_t fiftyoneDegreesUnsetMemoryTracking() {
	size_t memAlloced = MemoryTrackingGetAllocated();
	WRITE_STANDARD_FUNCTIONS();
	fiftyoneDegreesMemoryTrackingReset();
	return memAlloced;
}

void fiftyoneDegreesMemoryTrackingStartMeasuring() {
	ensureSetUp();
	// The counts are reset in the same section that starts the measurement,
	// so no other thread's allocation is counted in between.
	STATE_LOCK();
	state.allocated = 0;
	state.max = 0;
#ifndef FIFTYONE_DEGREES_NO_THREADING
	state.measurer = FIFTYONE_DEGREES_THREAD_ID_CURRENT();
	state.measuring = true;
#endif
	STATE_UNLOCK();
	WRITE_TRACKING_FUNCTIONS();
}

void fiftyoneDegreesMemoryTrackingStopMeasuring() {
	WRITE_STANDARD_FUNCTIONS();
	ensureSetUp();
	STATE_LOCK();
#ifndef FIFTYONE_DEGREES_NO_THREADING
	state.measuring = false;
#endif
	state.allocated = 0;
	state.max = 0;
	STATE_UNLOCK();
}

#ifdef FIFTYONE_DEGREES_MEMORY_TRACK_ENABLED

/**
 * Enable memory tracking.
 */

void *(FIFTYONE_DEGREES_CALL_CONV *fiftyoneDegreesMalloc)(size_t size) =
fiftyoneDegreesMemoryTrackingMalloc;

void* (FIFTYONE_DEGREES_CALL_CONV* fiftyoneDegreessMallocAligned)(
	int alignment,
	size_t size) = fiftyoneDegreesMemoryTrackingMallocAligned;

void (FIFTYONE_DEGREES_CALL_CONV *fiftyoneDegreesFree)(void* pointer) =
fiftyoneDegreesMemoryTrackingFree;

void (FIFTYONE_DEGREES_CALL_CONV *fiftyoneDegreesFreeAligned)(void* pointer) =
fiftyoneDegreesMemoryTrackingFreeAligned;

#else

/**
 * Disable memory tracking.
 */

void* (FIFTYONE_DEGREES_CALL_CONV* fiftyoneDegreesMalloc)(size_t size) =
fiftyoneDegreesMemoryStandardMalloc;

void* (FIFTYONE_DEGREES_CALL_CONV* fiftyoneDegreesMallocAligned)(
	int alignment,
	size_t size) = fiftyoneDegreesMemoryStandardMallocAligned;

void (FIFTYONE_DEGREES_CALL_CONV *fiftyoneDegreesFree)(void *pointer) =
fiftyoneDegreesMemoryStandardFree;

void (FIFTYONE_DEGREES_CALL_CONV *fiftyoneDegreesFreeAligned)(void* pointer) =
fiftyoneDegreesMemoryStandardFreeAligned;

#endif