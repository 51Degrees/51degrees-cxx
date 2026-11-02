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

#ifndef FIFTYONE_DEGREES_RESULTS_INCLUDED
#define FIFTYONE_DEGREES_RESULTS_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

/**
 * @ingroup FiftyOneDegreesCommon
 * @defgroup FiftyOneDegreesResults Results
 *
 * Structure returned by an engine's process method(s), containing values.
 *
 * ## Introduction
 *
 * Results are the structure returned by an engine's process method(s) and
 * contains the values for the properties in the engine.
 * The base structure contains only the essential element of a data set, which
 * is needed to return values. Any engine can extend this structure to add any
 * additional elements which may be needed to format or return values.
 *
 * The data set pointer is a managed resource, meaning that while the results
 * exist, the data set will not be freed by the resource manager. This ensures
 * that values contained in the data set used for processing can always be
 * returned.
 *
 * ## First read
 *
 * When the caller named the properties the data set was created with,
 * populating the results walks the graph of each component they answer for,
 * as #fiftyoneDegreesResultsWalkWhenPopulated decides, and the call that
 * populates them reports a walk that fails. Otherwise populating the results
 * walks no graph. Each component the results answer for is marked pending,
 * and the first read of one of its properties walks the component's graph
 * against the data set the results hold. A component is walked at most once
 * each time the results are populated, so a walk that fails is not repeated,
 * and the component's properties then read as if it had no profile. Anything
 * that depends on every component, such as a value made from all of them or
 * the items of the results read directly, needs
 * #fiftyoneDegreesResultsEvaluateAll first, which walks nothing when the
 * results walked every graph as they were populated.
 *
 * ## Overrides
 *
 * Results can hold values from the evidence that override the values of
 * properties. A read looks for one with
 * #fiftyoneDegreesResultsGetOverrideValues before it walks the property's
 * component, so a property with an override value is read without walking
 * the graph, and a component whose properties are only read from overrides
 * is never walked.
 *
 * ## Threads
 *
 * A results structure is used by one thread at a time, because reading a
 * value writes into the results, and when no properties were named the first
 * read of a component's property walks the component's graph. Different
 * results can be used on different threads at once, and the resource manager
 * they come from can be shared, unless the library is compiled with
 * FIFTYONE_DEGREES_NO_THREADING.
 *
 * @{
 */

#include <string.h>
#include "data.h"
#include "status.h"
#include "dataset.h"
#include "common.h"

/**
 * Enum containing reasons which cause a value to not be present or valid.
 */
typedef enum e_fiftyone_degrees_results_no_value_reason {
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_DIFFERENCE, /**< The difference
														 value is higher than
														 the threshold, see
														 the Pattern API */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_MATCHED_NODES, /**< No hash
															   nodes were
															   matched, see the
															   Hash API */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY, /**< The requested
															   property does
															   not exist, or is
															   not a required
															   property */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULT_FOR_PROPERTY, /**< There
																	 is no result
																	 which
																	 contains a
																	 value for
																	 the requested
																	 property */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS, /**< There are no
														 results to get a value
														 from */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_TOO_MANY_VALUES, /**< There are too
															  many values to be
															  expressed as the
															  requested type */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE, /**< The results
														   contain a null
														   profile for the
														   required component */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_HIGH_RISK, /**< The match is
	                                                    deemed high risk of
														containing incorrect or
														misleading results. */
	FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN /**< None of the above */
} fiftyoneDegreesResultsNoValueReason;

struct fiftyone_degrees_results_base_t;

/**
 * Walks the graph of one component for the results, setting what a read of
 * the component's properties needs. Each engine supplies its own walk.
 * @param results pointer to the results to walk the component for
 * @param componentIndex index of the component in the data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
typedef void (*fiftyoneDegreesResultsEvaluateMethod)(
	struct fiftyone_degrees_results_base_t *results,
	byte componentIndex,
	fiftyoneDegreesException *exception);

/**
 * Number of 32 bit words holding one bit for each possible component index.
 */
#define FIFTYONE_DEGREES_RESULTS_PENDING_WORDS 8

/**
 * Base results structure which any processing results should extend. It is
 * used by one thread at a time, as the Threads section above describes.
 */
typedef struct fiftyone_degrees_results_base_t {
	void *dataSet; /**< Pointer to the data set associated with the results */
	fiftyoneDegreesResultsEvaluateMethod evaluate; /**< Walks one component's
												   graph, or NULL if the
												   results walk none on a
												   read */
	uint32_t pending[FIFTYONE_DEGREES_RESULTS_PENDING_WORDS]; /**< One bit for
															  each component
															  whose graph is
															  still to be
															  walked */
	fiftyoneDegreesOverrideValueArray *overrides; /**< Values from the
												  evidence that override the
												  values of properties, or
												  NULL if the results hold
												  none */
} fiftyoneDegreesResultsBase;

/**
 * Initialise a set of results by setting the data set they are associated with.
 * No component is pending, no walk is set and no override values are held.
 * @param results pointer to the results to initialise
 * @param dataSet pointer to the data set which will be using the results
 * @return pointer to the initialised results
 */
EXTERNAL fiftyoneDegreesResultsBase* fiftyoneDegreesResultsInit(
	fiftyoneDegreesResultsBase *results,
	void *dataSet);

/**
 * Marks a function defined in this header, so that it adds no name to the
 * libraries.
 */
#if defined(_MSC_VER)
#define FIFTYONE_DEGREES_RESULTS_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define FIFTYONE_DEGREES_RESULTS_INLINE static __inline__
#else
#define FIFTYONE_DEGREES_RESULTS_INLINE static inline
#endif

/**
 * Marks every component as walked, before the results are populated again.
 * @param results pointer to the results
 */
FIFTYONE_DEGREES_RESULTS_INLINE void fiftyoneDegreesResultsPendingClear(
	fiftyoneDegreesResultsBase *results) {
	memset(results->pending, 0, sizeof(results->pending));
}

/**
 * Marks the component as waiting for the first read of one of its
 * properties.
 * @param results pointer to the results
 * @param componentIndex index of the component in the data set
 */
FIFTYONE_DEGREES_RESULTS_INLINE void fiftyoneDegreesResultsPendingSet(
	fiftyoneDegreesResultsBase *results,
	byte componentIndex) {
	results->pending[componentIndex >> 5] |=
		(uint32_t)1 << (componentIndex & 31);
}

/**
 * Returns true if populating the results walks the graph of each component
 * they answer for, rather than leaving each walk to the first read of one of
 * the component's properties. That is when the caller named the properties
 * the data set was created with, as every graph the results need is then
 * known before anything is read.
 * @param dataSet pointer to the data set the results are populated from
 * @return true if populating the results walks the graphs
 */
FIFTYONE_DEGREES_RESULTS_INLINE bool fiftyoneDegreesResultsWalkWhenPopulated(
	const fiftyoneDegreesDataSetBase *dataSet) {
	return dataSet->available != NULL && dataSet->available->named != NULL;
}

/**
 * Walks the component's graph if it is pending, and marks it walked whether
 * or not the walk succeeds, so a failed walk is not repeated. Called before
 * a read of a property of the component.
 * @param results pointer to the results
 * @param componentIndex index of the component in the data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
FIFTYONE_DEGREES_RESULTS_INLINE void fiftyoneDegreesResultsEvaluate(
	fiftyoneDegreesResultsBase *results,
	byte componentIndex,
	fiftyoneDegreesException *exception) {
	uint32_t *word = &results->pending[componentIndex >> 5];
	const uint32_t bit = (uint32_t)1 << (componentIndex & 31);
	if ((*word & bit) != 0) {
		*word &= ~bit;
		if (results->evaluate != NULL) {
			results->evaluate(results, componentIndex, exception);
		}
	}
}

/**
 * Walks every pending component in component order, stopping at the first
 * walk that fails. Called before anything that depends on every component,
 * such as a value made from all of them, or the items of the results read
 * directly.
 * @param results pointer to the results
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
FIFTYONE_DEGREES_RESULTS_INLINE void fiftyoneDegreesResultsEvaluateAll(
	fiftyoneDegreesResultsBase *results,
	fiftyoneDegreesException *exception) {
	uint32_t word, bit;
	for (word = 0; word < FIFTYONE_DEGREES_RESULTS_PENDING_WORDS; word++) {
		while (results->pending[word] != 0) {
			bit = 0;
			while ((results->pending[word] & ((uint32_t)1 << bit)) == 0) {
				bit++;
			}
			fiftyoneDegreesResultsEvaluate(
				results,
				(byte)(word * 32 + bit),
				exception);
			if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
				return;
			}
		}
	}
}

/**
 * Returns true if the results hold an override value for the property, so
 * that a read can answer without walking the property's component.
 * @param results pointer to the results to check
 * @param availableIndex index of the property in the available properties
 * @return true if there is an override value for the property
 */
FIFTYONE_DEGREES_RESULTS_INLINE bool fiftyoneDegreesResultsHasOverrideValue(
	fiftyoneDegreesResultsBase *results,
	uint32_t availableIndex) {
	return results->overrides != NULL &&
		fiftyoneDegreesOverrideHasValueForRequiredPropertyIndex(
			results->overrides,
			availableIndex);
}

/**
 * Adds the override values the results hold for the property to the list.
 * Each read calls this before #fiftyoneDegreesResultsEvaluate, so a property
 * with an override value is read without walking its component.
 * @param results pointer to the results to get the values from
 * @param availableIndex index of the property in the available properties
 * @param values list to add the values to
 * @return the number of values added, which is zero if there is no override
 * value for the property
 */
FIFTYONE_DEGREES_RESULTS_INLINE uint32_t fiftyoneDegreesResultsGetOverrideValues(
	fiftyoneDegreesResultsBase *results,
	uint32_t availableIndex,
	fiftyoneDegreesList *values) {
	return results->overrides == NULL ? 0 :
		fiftyoneDegreesOverrideValuesAdd(
			results->overrides,
			availableIndex,
			values);
}

/**
 * @}
 */

#endif