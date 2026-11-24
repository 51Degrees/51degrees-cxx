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

#include "resource.h"
#include "fiftyone.h"

#define CLAIMED FIFTYONE_DEGREES_RESOURCE_CLAIMED
#define IN_USE_MAX FIFTYONE_DEGREES_RESOURCE_IN_USE_MAX

#ifndef FIFTYONE_DEGREES_NO_THREADING
/*
 * Reads a handle's counter as one atomic load, as other threads change it
 * with compare and exchange at the same time. Visual Studio reads an aligned
 * volatile long in one access, so a plain read is atomic there. On x86 and
 * x64 it is also ordered after the interlocked operations, but on ARM64,
 * where /volatile:iso is the default, it isn't. The loops below don't depend
 * on that order: each value they read is checked by the compare and exchange
 * that follows, which fails and tries again if the value is stale.
 */
#ifdef _MSC_VER
#define COUNTER_READ(v) (v)
#else
#define COUNTER_READ(v) __atomic_load_n(&(v), __ATOMIC_SEQ_CST)
#endif
#endif

/*
 * Reads the manager's active handle as one atomic load, as another thread can
 * replace it with compare and exchange at the same time. The load also orders
 * the reads of the handle after the writes that set it up, which a plain read
 * of the pointer doesn't. Visual Studio reads it through a volatile pointer.
 * Under /volatile:ms, the default on x86 and x64, a volatile read has acquire
 * semantics, so it gives the same order.
 */
#ifdef FIFTYONE_DEGREES_NO_THREADING
#define ACTIVE_READ(m) ((m)->active)
#elif defined(_MSC_VER)
#define ACTIVE_READ(m) (*(volatile ResourceHandle* volatile*)&(m)->active)
#else
#define ACTIVE_READ(m) __atomic_load_n(&(m)->active, __ATOMIC_SEQ_CST)
#endif

/**
 * Returns the number of users a counter holds, without the claimed bit.
 * @param counter value read from a handle
 * @return the number of users
 */
static int32_t getInUse(long counter) {
	return (int32_t)(counter & IN_USE_MAX);
}

/**
 * A handle and what keeps its memory allocated. A thread can read the active
 * handle from the manager just before another thread replaces it, then read
 * the handle after its last reference has gone. So the resource is freed with
 * the last reference, but the memory of the handle stays until the manager is
 * freed as well, which costs one of these for each replacement.
 *
 * Nothing limits how long a thread can pause between reading the active handle
 * and adding to its counter, so a handle can still be read after several more
 * replacements, and freeing it at a later replacement is not safe. Knowing that
 * no thread still holds one would need every lookup to update a second counter
 * shared by all threads. That is a cost on every lookup to save a few tens of
 * bytes for each replacement, so the memory is kept until the manager is freed.
 */
typedef struct retained_handle_t {
	ResourceHandle handle; /**< First, so a handle can be cast to this type */
	ResourceHandle *replaced; /**< The handle this one replaced, or NULL */
	volatile long owners; /**< Owners still holding the memory */
} RetainedHandle;

/** The owners of the memory of a handle: its resource and the manager. */
#define HANDLE_MEMORY_OWNERS 2

/**
 * Releases one owner's hold on the memory of a handle, and frees the memory
 * if that was the last owner.
 * @param handle whose memory is no longer needed by the owner
 */
static void releaseHandleMemory(ResourceHandle *handle) {
	if (INTERLOCK_DEC(&((RetainedHandle*)handle)->owners) == 0) {
		FreeAligned((void*)handle);
	}
}

/**
 * Returns the handle to the resource that is ready to be set for the manager,
 * or NULL if the handle was not successfully created.
 * @param manager of the resource
 * @param resource to be assigned to the manager
 * @parma resourceHandle reference to the handle within the resource
 */
static void setupResource(
	ResourceManager *manager, 
	void *resource, 
	ResourceHandle **resourceHandle,
	void(*freeResource)(void*)) {

	// Create a new active handle for the manager.
	RetainedHandle *retained = (RetainedHandle*)
		MallocAligned(
			sizeof(void*),
			sizeof(RetainedHandle));
	ResourceHandle *handle = &retained->handle;
	retained->replaced = NULL;
	retained->owners = HANDLE_MEMORY_OWNERS;

	// The handle has no users and is not claimed.
	handle->counter = 0;

	// Set a link between the new active resource and the manager. Used to
	// check if the resource can be freed when the last thread has finished
	// using it.
	handle->manager = manager;

	// Set the resource that the new handle manages to the one provided.
	handle->resource = resource;

	// Set the free resource method as this may not be available if the manager
	// is disposed of.
	handle->freeResource = freeResource;

	// Ensure the resource's handle is set before assigning the handle
	// as the active handle.
	*resourceHandle = handle;
}

static void freeHandle(volatile ResourceHandle *handle) {
	handle->freeResource((void*)handle->resource);
	releaseHandleMemory((ResourceHandle*)handle);
}

void fiftyoneDegreesResourceManagerInit(
	fiftyoneDegreesResourceManager *manager,
	void *resource,
	fiftyoneDegreesResourceHandle **resourceHandle,
	void(*freeResource)(void*)) {

	// Initialise the manager with the resource ensuring that the resources
	// handle is set before it's made the active resource.
	setupResource(manager, resource, resourceHandle, freeResource);
	manager->active = *resourceHandle;
}

void fiftyoneDegreesResourceManagerFree(
	fiftyoneDegreesResourceManager *manager) {
	// Unlike IncUse and DecUse, Free should not be
	// called at the same time as a reload so the 
	// active handle won't change at this point.
	// Thus, it is safe to perform assertion directly
	// to the active handle here. 
	assert(manager->active == NULL ||
		(manager->active->counter & CLAIMED) == 0);
	if (manager->active != NULL) {

		ResourceHandle* newHandlePointer;
		ResourceHandle* handle;
		ResourceHandle* replaced;
		fiftyoneDegreesResourceReplace(
			manager,
			NULL,
			&newHandlePointer);

		// Nothing may read from a manager being freed, so it can let go of
		// every handle it has replaced. A handle still in use is freed when
		// its last reference goes.
		for (handle = ((RetainedHandle*)newHandlePointer)->replaced;
			handle != NULL;
			handle = replaced) {
			replaced = ((RetainedHandle*)handle)->replaced;
			releaseHandleMemory(handle);
		}
		FreeAligned(newHandlePointer);
	}
}

void fiftyoneDegreesResourceHandleDecUse(
	fiftyoneDegreesResourceHandle *handle) {
	// When modifying this method, it is important to note the reason for using
	// two separate compare and swaps. The first compare and swap ensures that
	// we are certain the handle is ready to be released i.e. the in use count
	// is zero, and the handle is no longer active in the manager. The second
	// compare and swap ensures that we are certain the handle can be freed by
	// THIS thread. See below for an example of when this can happen.
	long decremented;
#ifndef FIFTYONE_DEGREES_NO_THREADING
	long compare;
	do {
		compare = COUNTER_READ(handle->counter);
		assert(getInUse(compare) > 0);
		assert((compare & CLAIMED) == 0);
		decremented = compare - 1;
	} while (FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(
		handle->counter,
		decremented,
		compare) != compare);
#else
	assert(getInUse(handle->counter) > 0);
	handle->counter--;
	decremented = handle->counter;
#endif
	if (decremented == 0 &&  // Am I the last user of the handle?
		ACTIVE_READ(handle->manager) != handle) { // Is the handle still active?
#ifndef FIFTYONE_DEGREES_NO_THREADING
		// Atomically set the claimed bit to ensure only one thread can get
		// into the freeHandle method.
		// Consider the scenario where between the decrement this if statement:
		// 1. another thread increments and decrements the counter, then
		// 2. the active handle is replaced.
		// In this case, both threads will make it into here, so access to
		// the freeHandle method must be limited to one by atomically setting
		// the claimed bit. Only the thread whose swap sets it frees the
		// handle, and once it is set no thread can take the handle again.
		if (FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(
			handle->counter,
			CLAIMED,
			0L) == 0L) {
			assert(handle != ACTIVE_READ(handle->manager));
			freeHandle(handle);
		}
#else
		freeHandle(handle);
#endif
	}
}

fiftyoneDegreesResourceHandle* fiftyoneDegreesResourceHandleIncUse(
	fiftyoneDegreesResourceManager *manager) {
#ifndef FIFTYONE_DEGREES_NO_THREADING
	long compare;
	volatile ResourceHandle* active;
	for (;;) {
		// Read the active handle once for each attempt, so the counter and
		// the exchange use the same handle.
		active = ACTIVE_READ(manager);
		compare = COUNTER_READ(active->counter);
		// A claimed handle was replaced and its resource freed before it was
		// read, so start again from the manager.
		if ((compare & CLAIMED) != 0) {
			continue;
		}
		// The count cannot go past its bits, so wait for another thread to
		// release the handle.
		if (getInUse(compare) == IN_USE_MAX) {
			continue;
		}
		if (FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(
			active->counter,
			compare + 1,
			compare) == compare) {
			return (ResourceHandle*)active;
		}
	}
#else
	// A single thread cannot wait for itself to release the handle.
	assert(getInUse(manager->active->counter) < IN_USE_MAX);
	manager->active->counter++;
	return manager->active;
#endif
}

int32_t fiftyoneDegreesResourceHandleGetUse(
	fiftyoneDegreesResourceHandle *handle) {
	if (handle != NULL) {
		return getInUse(handle->counter);
	}
	else {
		return 0;
	}
}

void fiftyoneDegreesResourceReplace(
	fiftyoneDegreesResourceManager *manager,
	void *newResource,
	fiftyoneDegreesResourceHandle **newResourceHandle) {
	// Nothing is read through this pointer here, it is only compared and
	// passed on, so it has the same type as the functions it is passed to.
	ResourceHandle* oldHandle = NULL;
	
	// Add the new resource to the manager replacing the existing one.
	setupResource(
		manager,
		newResource,
		newResourceHandle,
		ACTIVE_READ(manager)->freeResource);
	assert((*newResourceHandle)->counter == 0);
#ifndef FIFTYONE_DEGREES_NO_THREADING
	// Switch the active handle for the manager to the newly created one. If
	// another thread switched it first, the swap fails, so take the handle
	// that is now active and try again.
	do {
		if (oldHandle != NULL) {
			ResourceHandleDecUse(oldHandle);
		}
		oldHandle = ResourceHandleIncUse(manager);
		((RetainedHandle*)*newResourceHandle)->replaced =
			(ResourceHandle*)oldHandle;
	} while (INTERLOCK_EXCHANGE_PTR(
		manager->active,
		*newResourceHandle,
		oldHandle) != oldHandle);
#else
	oldHandle = ResourceHandleIncUse(manager);
	((RetainedHandle*)*newResourceHandle)->replaced = oldHandle;
	manager->active = *newResourceHandle;
#endif
	// Release this thread's use of the replaced resource. It is freed once
	// nothing else holds it.
	ResourceHandleDecUse(oldHandle);
}