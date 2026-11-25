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

#define ACTIVE FIFTYONE_DEGREES_RESOURCE_ACTIVE
#define IN_USE_MAX FIFTYONE_DEGREES_RESOURCE_IN_USE_MAX

#ifdef FIFTYONE_DEGREES_NO_THREADING
#define COUNTER_READ(v) (v)
#else
/*
 * Reads a handle's counter as one atomic load, as other threads change it at
 * the same time. The read needs no ordering of its own, so gcc and clang use
 * a relaxed load. Visual Studio reads an aligned volatile long in one access,
 * so a plain read is atomic there. The loop that reads it doesn't depend on
 * order: a value it reads is either checked by the compare and exchange that
 * follows, which fails and tries again if the value is stale, or makes it
 * start again from the manager. GetUse only returns the value it reads,
 * which may already be out of date.
 */
#ifdef _MSC_VER
#define COUNTER_READ(v) (v)
#else
#define COUNTER_READ(v) __atomic_load_n(&(v), __ATOMIC_RELAXED)
#endif
#endif

/*
 * Reads the manager's active handle as one atomic load, as another thread can
 * replace it with compare and exchange at the same time. The load also orders
 * the reads of the handle after the writes that set it up, which a plain read
 * of the pointer doesn't. Visual Studio reads it through a volatile pointer,
 * for the same reasons as the counter above.
 */
#ifdef FIFTYONE_DEGREES_NO_THREADING
#define ACTIVE_READ(m) ((m)->active)
#elif defined(_MSC_VER)
#define ACTIVE_READ(m) (*(volatile ResourceHandle* volatile*)&(m)->active)
#else
#define ACTIVE_READ(m) __atomic_load_n(&(m)->active, __ATOMIC_SEQ_CST)
#endif

/**
 * Returns the number of users a counter holds, without the active bit.
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
 * freed as well, which costs one of these for each replacement. The memory
 * also stays until the release of the last reference has finished with it.
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

	// The handle has no users, and the active bit holds it for the manager.
	handle->counter = ACTIVE;

	// Set a link between the new active resource and the manager.
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
	// Free must not run at the same time as a take or a replace, so the
	// active handle cannot change here. Releases can run at any time.
	assert(manager->active == NULL ||
		(COUNTER_READ(manager->active->counter) & ACTIVE) != 0);
	if (manager->active != NULL) {

		ResourceHandle* newHandlePointer;
		ResourceHandle* handle;
		ResourceHandle* replaced;
		fiftyoneDegreesResourceReplace(
			manager,
			NULL,
			&newHandlePointer);

		// Nothing may take a handle from a manager being freed, so it can let
		// go of every handle it has replaced. The memory of a handle still in
		// use is freed when its last reference goes.
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
	// The active bit counts as a use, so the counter reaches zero only at the
	// last release of a handle that has been replaced. A handle at zero is
	// never taken again, so only one release sees zero, and the handle's
	// memory is kept until that release has freed the resource. Nothing here
	// reads the manager, which may already have been freed.
	long decremented;
#ifndef FIFTYONE_DEGREES_NO_THREADING
	decremented = FIFTYONE_DEGREES_INTERLOCK_DEC(&handle->counter);
#else
	assert(getInUse(handle->counter) > 0);
	decremented = --handle->counter;
#endif
	assert(decremented >= 0);
	if (decremented == 0) {
		freeHandle(handle);
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
		// Only a handle with the active bit set, and fewer than IN_USE_MAX
		// users, can be taken, which is one unsigned comparison. A handle
		// without the bit was replaced after it was read. A full handle needs
		// another thread to release it first. Either way, start again from
		// the manager. The exchange fails if the bit is cleared meanwhile.
		if ((unsigned long)(compare - ACTIVE) >= (unsigned long)IN_USE_MAX) {
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
	assert((manager->active->counter & ACTIVE) != 0 &&
		getInUse(manager->active->counter) < IN_USE_MAX);
	manager->active->counter++;
	return manager->active;
#endif
}

int32_t fiftyoneDegreesResourceHandleGetUse(
	fiftyoneDegreesResourceHandle *handle) {
	if (handle != NULL) {
		return getInUse(COUNTER_READ(handle->counter));
	}
	else {
		return 0;
	}
}

void fiftyoneDegreesResourceReplace(
	fiftyoneDegreesResourceManager *manager,
	void *newResource,
	fiftyoneDegreesResourceHandle **newResourceHandle) {
	ResourceHandle* oldHandle;
	long counter;

	// Add the new resource to the manager replacing the existing one.
	setupResource(
		manager,
		newResource,
		newResourceHandle,
		ACTIVE_READ(manager)->freeResource);
#ifndef FIFTYONE_DEGREES_NO_THREADING
	// Switch the active handle for the manager to the newly created one. If
	// another thread switched it first, the swap fails, so try again with the
	// handle that is now active.
	do {
		oldHandle = (ResourceHandle*)ACTIVE_READ(manager);
		((RetainedHandle*)*newResourceHandle)->replaced = oldHandle;
	} while (INTERLOCK_EXCHANGE_PTR(
		manager->active,
		*newResourceHandle,
		oldHandle) != oldHandle);

	// Only the thread whose swap removed the old handle clears its active
	// bit. Users can take and release it until then, so retry if the counter
	// changes.
	do {
		counter = COUNTER_READ(oldHandle->counter);
	} while (FIFTYONE_DEGREES_INTERLOCK_EXCHANGE(
		oldHandle->counter,
		counter - ACTIVE,
		counter) != counter);
#else
	oldHandle = manager->active;
	((RetainedHandle*)*newResourceHandle)->replaced = oldHandle;
	manager->active = *newResourceHandle;
	counter = oldHandle->counter;
	oldHandle->counter -= ACTIVE;
#endif
	assert((counter & ACTIVE) != 0);

	// Free the replaced resource if it has no users. Otherwise its last
	// release frees it.
	if (counter == ACTIVE) {
		freeHandle(oldHandle);
	}
}
