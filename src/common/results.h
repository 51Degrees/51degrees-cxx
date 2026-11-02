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
 * Populating the results walks no graph. Each component the results answer
 * for is marked pending, and the first read of one of its properties walks
 * the component's graph against the data set the results hold. A component
 * is walked at most once each time the results are populated, so a walk that
 * fails is not repeated, and the component's properties then read as if it
 * had no profile. Anything that depends on every component, such as a value
 * made from all of them or the items of the results read directly, needs
 * #fiftyoneDegreesResultsEvaluateAll first.
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
 * Base results structure which any processing results should extend.
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
} fiftyoneDegreesResultsBase;

/**
 * Initialise a set of results by setting the data set they are associated with.
 * No component is pending and no walk is set.
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
 * @}
 */

#endif