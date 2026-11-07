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

#include "pool.h"
#include "fiftyone.h"

#ifndef FIFTYONE_DEGREES_NO_THREADING

/*
 * Reads a value shared between threads after any change this thread made
 * before it. A waiting thread raises the count and then reads the head, and
 * a releasing thread changes the head and then reads the count, so each sees
 * the other's change. The interlocked operations are full barriers with
 * Visual Studio, so a plain read is ordered after them there.
 */
#ifdef _MSC_VER
#define POOL_READ(v) (v)
#else
#define POOL_READ(v) __atomic_load_n(&(v), __ATOMIC_SEQ_CST)
#endif

static void poolWaitCreate(Pool *pool) {
	pool->waiting = 0;
	FIFTYONE_DEGREES_MUTEX_CREATE(pool->lock);
#ifdef _MSC_VER
	InitializeConditionVariable(&pool->released);
#else
	pthread_cond_init(&pool->released, NULL);
#endif
}

static void poolWaitClose(Pool *pool) {
#ifndef _MSC_VER
	pthread_cond_destroy(&pool->released);
#endif
	FIFTYONE_DEGREES_MUTEX_CLOSE(pool->lock);
}

// Sleeps until a release wakes the thread. The lock must be held.
static void poolWait(Pool *pool) {
#ifdef _MSC_VER
	SleepConditionVariableSRW(&pool->released, &pool->lock, INFINITE, 0);
#else
	pthread_cond_wait(&pool->released, &pool->lock);
#endif
}

static void poolWake(Pool *pool) {
	FIFTYONE_DEGREES_MUTEX_LOCK(&pool->lock);
#ifdef _MSC_VER
	WakeConditionVariable(&pool->released);
#else
	pthread_cond_signal(&pool->released);
#endif
	FIFTYONE_DEGREES_MUTEX_UNLOCK(&pool->lock);
}

#endif

fiftyoneDegreesPool* fiftyoneDegreesPoolInit(
	fiftyoneDegreesPool *pool,
	uint16_t concurrency,
	void *state,
	fiftyoneDegreesPoolResourceCreate resourceCreate,
	fiftyoneDegreesPoolResourceFree resourceFree,
	fiftyoneDegreesException *exception) {
	uint16_t i = 1;
	PoolItem *item;

	// Add one to the concurrency value so that a NULL marker can be
	// written as the last item in the linked list that if returned
	// indicates that every item is in use.
	uint16_t listItems = concurrency + 1;

	// Set the stack and head of the linked list.
	pool->count = 0;
	pool->head.exchange = 0;
	pool->resourceFree = resourceFree;

	// Allocate memory for the stack.
	pool->stack = (PoolItem*)Malloc(sizeof(PoolItem) * listItems);
	if (pool->stack != NULL) {
#ifndef FIFTYONE_DEGREES_NO_THREADING
		poolWaitCreate(pool);
#endif

		// The entry at index 0 in the stack is the null item which if ever
		// retrieved indicates that every item is in use. It does not contain
		// a value resource.
		item = &pool->stack[0];
		item->pool = pool;
		item->resource = NULL;

		// Initialise all the resources in the pool after the null terminator.
		while (i < listItems && EXCEPTION_OKAY) {
			item = &pool->stack[i];
			item->pool = pool;
			item->resource = resourceCreate(pool, state, exception);
			pool->count++;
			item->next = pool->head.values.index;
			pool->head.values.index = i;
			i++;
		}
	}
	else {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
	}

	return pool;
}

/**
 * Takes the item at the head of the stack.
 * @param pool to take the item from
 * @return the item, or NULL if every item is in use
 */
static PoolItem* poolPop(Pool *pool) {
	PoolHead orig;
#ifndef FIFTYONE_DEGREES_NO_THREADING
	PoolHead next;
	do {
		orig.exchange = POOL_READ(pool->head.exchange);
#else
		orig = pool->head;
#endif

		// The null item at the head shows that every item is in use.
		if (pool->stack[orig.values.index].resource == NULL) {
			return NULL;
		}

#ifndef FIFTYONE_DEGREES_NO_THREADING
		next.values.aba = orig.values.aba + 1;
		next.values.index = pool->stack[orig.values.index].next;
	} while (INTERLOCK_EXCHANGE(
		pool->head.exchange,
		next.exchange,
		orig.exchange) != orig.exchange);
#else
		pool->head.values.index = pool->stack[orig.values.index].next;
#endif
	return &pool->stack[orig.values.index];
}

fiftyoneDegreesPoolItem* fiftyoneDegreesPoolItemGet(
	fiftyoneDegreesPool *pool,
	fiftyoneDegreesException *exception) {
	PoolItem *item = poolPop(pool);
	if (item == NULL) {
#ifndef FIFTYONE_DEGREES_NO_THREADING
		// Wait for another thread to release an item. The count is raised
		// before the stack is read again, so a release either finds the
		// count and wakes this thread, or pushes its item before the read.
		// Waiting cannot fail, so the exception is not used.
		(void)exception;
		FIFTYONE_DEGREES_MUTEX_LOCK(&pool->lock);
		INTERLOCK_INC(&pool->waiting);
		while ((item = poolPop(pool)) == NULL) {
			poolWait(pool);
		}
		INTERLOCK_DEC(&pool->waiting);
		FIFTYONE_DEGREES_MUTEX_UNLOCK(&pool->lock);
#else
		// The only thread holds every item, so none can be released.
		EXCEPTION_SET(INSUFFICIENT_HANDLES);
#endif
	}
	return item;
}

void fiftyoneDegreesPoolItemRelease(fiftyoneDegreesPoolItem *item) {
#ifndef FIFTYONE_DEGREES_NO_THREADING
	PoolHead orig, next;
	Pool *pool = item->pool;
	do {
		orig.exchange = POOL_READ(pool->head.exchange);
		item->next = orig.values.index;
		next.values.aba = orig.values.aba + 1;
		next.values.index = (uint16_t)(item - pool->stack);
	} while (INTERLOCK_EXCHANGE(
		pool->head.exchange,
		next.exchange,
		orig.exchange) != orig.exchange);
	if (POOL_READ(pool->waiting) > 0) {
		poolWake(pool);
	}
#else
	item->next = item->pool->head.values.index;
	item->pool->head.values.index =
		(uint16_t)(item - item->pool->stack);
#endif
}

void fiftyoneDegreesPoolReset(fiftyoneDegreesPool *pool) {
	pool->head.values.index = 0;
	pool->head.values.aba = 0;
	pool->stack = NULL;
	pool->count = 0;
	pool->resourceFree = NULL;
}

void fiftyoneDegreesPoolFree(fiftyoneDegreesPool *pool) {
	void *resource;
	uint16_t i;
	if (pool->stack != NULL) {
		if (pool->resourceFree != NULL) {
			for (i = 0; i <= pool->count; i++) {
				resource = pool->stack[i].resource;
				if (resource != NULL) {
					pool->resourceFree(pool, resource);
				}
			}
		}
		Free(pool->stack);
#ifndef FIFTYONE_DEGREES_NO_THREADING
		poolWaitClose(pool);
#endif
	}
	PoolReset(pool);
}
