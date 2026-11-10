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

/**
 * @ingroup FiftyOneDegreesCommon
 * @defgroup FiftyOneDegreesProperties Properties
 *
 * Structures for properties which are available, or required.
 *
 * ## Terms
 *
 * **Required Property Index** : the 0 based index of the property in the
 * names the caller gave when the data set was created, which is the index
 * every read by position takes. A reload does not change it. A name the data
 * does not have keeps its index, and reads as a property the data set does
 * not have. When the caller named no property, the names are those of the
 * first data set loaded, in the order of its list of available properties,
 * and a reload keeps them, so a property only a later data set has has no
 * index.
 *
 * **Available Index** : the 0 based index of the property in the list of
 * available properties, which holds the properties the data has in ascending
 * order of name. Not the same as the required property index when the caller
 * named properties.
 *
 * **Property Index** : the 0 based index of the property in the source used to 
 * create the required properties. Not the same as the required property index.
 *
 * **Results** : the structure used to contain the data associated with the 
 * available required properties.
 *
 * **Source** : the source of property names. Typically a call back method to
 * return properties from the data structure used by the aspect engine.
 *
 * ## Introduction
 *
 * Aspect engine performance can often be improved by limiting the properties
 * that can be returned from the engine. As this is a performance feature 
 * shared across different types of aspect engine the utility functions are
 * stored in the common C files.
 *
 * Required properties could be specified at initialisation as a character 
 * separated string, as an array of strings or from an existing set of 
 * properties already obtained from these methods. In all scenarios it is 
 * desirable to be able to query the properties and find their indexes in the 
 * source data structure using a consistent set of methods.
 *
 * ## Creation
 *
 * #fiftyoneDegreesPropertiesCreate is used to create a consistent set of 
 * required properties. The structure passed must have one of the follow set.
 *
 * 1. Separated string of property names. Valid separators include space,
 * comma and pipe.
 * 2. An array of strings.
 * 3. Existing properties results from a previous creation operation. This
 * method is used when a data set is being reloaded due to a change in the
 * underlying data. The new results are created from the names the caller
 * gave the first time, so a property the previous data did not have is
 * available once the new data has it. A caller that named no property gets
 * every property of the new data, with the names of the first data set as
 * its required property indexes.
 *
 * Creation also requires a method to retrieve the name of the properties from 
 * the underlying data source. A method pointer conforming to 
 * #fiftyoneDegreesPropertiesGetMethod must be provided to retrieve the
 * property name string item for a given property index up to the maximum
 * count. The call back is used to avoid any hard dependency between the data
 * structure and the properties methods.
 *
 * Unlike standard collection item retrieval the properties methods will not 
 * know the underlying collection implement or reference from which the item 
 * was obtained from. Therefore the implementation of get MUST set the 
 * collection field of the collection item.
 *
 * Some, or all, of the required properties may not be present in the
 * underlying data source. A missing property is left out of the available
 * properties, and keeps its required property index, which reads as a
 * property the data set does not have. The number of required property
 * indexes is given by #fiftyoneDegreesPropertiesGetRequiredPropertyCount, and
 * the name at each by #fiftyoneDegreesPropertiesGetNameFromRequiredIndex. The
 * count field and the items of the results are the available properties,
 * which are in available index order, so a caller pairing them with required
 * property indexes reads the wrong properties.
 *
 * Whilst the property names can be provided as string(s) the property names
 * referenced from the properties results are collection items from a strings
 * collection. The properties methods are responsible for releasing the
 * references to these strings when the properties results are freed.
 *
 * The available properties are in ascending order of the property name, so
 * a binary search quickly finds a property from its name.
 *
 * ## Free
 *
 * When the properties results are finished with, the 
 * #fiftyoneDegreesPropertiesFree method must be used. This will also free the 
 * collection items used to reference the property name strings using the 
 * collection field of the item to obtain the source collection. This
 * collection must have been set by the get method used at creation.
 *
 * ## Operation
 *
 * Several methods are available which can be used to convert from an input 
 * such as a property name or required property index to a property index in 
 * the data structure. The method names are self-explanatory.
 *
 * Where possible using integer property or required property indexes is 
 * preferable to the string representation of the property as this improves 
 * performance.
 *
 * @{
 */

#ifndef FIFTYONE_DEGREES_PROPERTIES_H_INCLUDED
#define FIFTYONE_DEGREES_PROPERTIES_H_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

#include <stdint.h>
#ifdef _MSC_FULL_VER
#include <string.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif
#include "string.h"
#include "list.h"
#include "data.h"
#include "collection.h"
#include "array.h"
#include "common.h"

/** Characters which separate the names in a required properties string. */
#define FIFTYONE_DEGREES_PROPERTIES_SEPARATORS "|, "

/**
 * Index in the properties collection to a property which is required to get
 * evidence for another property.
 */
typedef uint32_t fiftyoneDegreesEvidencePropertyIndex;

FIFTYONE_DEGREES_ARRAY_TYPE(fiftyoneDegreesEvidencePropertyIndex, )

/**
 * Array of properties which are required to fetch additional evidence for
 * a specific property.
 */
typedef fiftyoneDegreesEvidencePropertyIndexArray fiftyoneDegreesEvidenceProperties;

/** Used to access the property item quickly without the need to search. */
typedef struct fiftyone_degrees_property_available_t {
	uint32_t propertyIndex; /**< Index of the property in the collection of all 
	                           properties*/
	fiftyoneDegreesCollectionItem name; /**< Name of the property from strings */
    fiftyoneDegreesEvidenceProperties *evidenceProperties; /**< Evidence
                                                           properties which are
                                                           required by this
                                                           property */
    bool delayExecution; /**< True if the execution any JavaScript returned as
                         a value of this property should be delayed. False if
                         it should be run immediately. This is always
                         initialized to false, so should be set by the calling
                         function */
    byte componentIndex; /**< Index in the data set's component list of the
                         component the property belongs to. Initialised to
                         zero and set by the engine when it initialises its
                         available components. */
    int32_t requiredPropertyIndex; /**< Index of the property in the names
                                   the caller gave, the first if the name was
                                   given more than once, or -1 if it is not
                                   among them. When the caller named no
                                   property, the index of this item in the
                                   first data set loaded */
} fiftyoneDegreesPropertyAvailable;

/**
 * A property name the caller gave, in the place the caller gave it.
 */
typedef struct fiftyone_degrees_property_named_t {
	int32_t availableIndex; /**< Index of the property in the available
	                            properties, or -1 when the data does not have
	                            the property */
	const fiftyoneDegreesString *name; /**< The name as the caller gave it */
} fiftyoneDegreesPropertyNamed;

FIFTYONE_DEGREES_ARRAY_TYPE(
	fiftyoneDegreesPropertyAvailable,
	bool allComponentsRequired; /**< True if every component must be resolved
	                                whether or not it has an available property.
	                                Initialised to false and set by the engine.
	                                A list created from an existing list keeps
	                                the value, which carries it across a reload */
	bool namesFromDataSet; /**< True when the caller named no property, so the
	                           names are those of the first data set loaded.
	                           The data set then walks a component's graph at
	                           the first read of one of its properties, as
	                           when no names are held */
	uint32_t namedCount; /**< Number of names the caller gave, which is the
	                         number of required property indexes when the
	                         caller named properties */
	fiftyoneDegreesPropertyNamed *named; /**< The names the caller gave, in the
	                                         caller's order, or NULL when the
	                                         caller named no property and this
	                                         is the first data set. A list
	                                         created from an existing list is
	                                         created from these names */
)

/** Array of properties which are available in a data set. */
typedef fiftyoneDegreesPropertyAvailableArray 
fiftyoneDegreesPropertiesAvailable;

/**
 * Defines a set of properties which are required by a caller. Usually to a
 * data set creation method.
 */
EXTERNAL typedef struct fiftyone_degrees_properties_required_t {
	const char **array; /**< Array of required properties or NULL if all 
	                        properties are required. See the count property for
	                        the number of items in the array */
	int count; /**< Number of properties in array */
	const char *string; /**< Separated list of required properties or NULL if 
	                        all properties are required */
	fiftyoneDegreesPropertiesAvailable *existing; /**< A pointer to an existing
	                                                  set of property names
													  from another instance */
} fiftyoneDegreesPropertiesRequired;

/**
 * Returns a string for the property at the index provided or NULL if there
 * is no property available at the index.
 * @param state resource used to obtain the string
 * @param index of the property
 * @param item used to obtain a handle to the string
 * @return pointer to the string or NULL if no property available
 */
typedef const fiftyoneDegreesString*(*fiftyoneDegreesPropertiesGetMethod)(
	void *state,
	uint32_t index,
	fiftyoneDegreesCollectionItem *item);

/**
 * Populates the evidence properties structure with the indexes of the
 * properties required by the property provided, and returns the number
 * of property indexes which were added. If the evidence properties structure
 * is null, then this method returns the count but does not populate the
 * structure.
 * @param state pointer to data which the method may need to use
 * @param property pointer to the property to get the evidence properties for
 * @param evidenceProperties pointer to the pre-allocated structure to populate
 * with the evidence property indexes
 * @return the number of property indexes added to the structure. Or the number
 * which would have been added were it not null
 */
typedef uint32_t(*fiftyoneDegreesEvidencePropertiesGetMethod)(
    void* state,
    fiftyoneDegreesPropertyAvailable* property,
    fiftyoneDegreesEvidenceProperties* evidenceProperties);

/**
 * The default properties required to make all possible properties available.
 * Should be used to initialise a new instance of 
 * #fiftyoneDegreesPropertiesRequired.
 */
EXTERNAL_VAR fiftyoneDegreesPropertiesRequired fiftyoneDegreesPropertiesDefault;

/**
 * Creates a properties result instance for use with future property
 * operations. The available properties are in ascending order of name, and
 * the required property index of each name given is its place among the
 * names given.
 * @param properties required to be available as either a separated list of
 * property names, an array of property names or an existing properties results
 * structure from another data set. Valid separators include pipe, comma and
 * space, and a run of separators separates two names. The fields are
 * evaluated in order of existing, array and then string. The first field
 * with a value is used. Existing results give the names they were created
 * with.
 * @param state pointer to state used with the get method
 * @param getPropertyMethod method used to return the property name from a 
 * string collection
 * @param getEvidencePropertiesMethod method used to populate the evidence
 * properties for a property
 * @return instance of a properties result for use with future properties
 * methods
 */
EXTERNAL fiftyoneDegreesPropertiesAvailable* fiftyoneDegreesPropertiesCreate(
	fiftyoneDegreesPropertiesRequired *properties,
	void *state,
	fiftyoneDegreesPropertiesGetMethod getPropertyMethod,
    fiftyoneDegreesEvidencePropertiesGetMethod getEvidencePropertiesMethod);

/**
 * Gets the index of the property in the source data structure from the name.
 * @param available properties instance
 * @param propertyName string containing the property name
 * @return 0 based index of the property in the source or -1 if not available
 */
EXTERNAL int fiftyoneDegreesPropertiesGetPropertyIndexFromName(
	fiftyoneDegreesPropertiesAvailable *available,
	const char *propertyName);

/**
 * Gets the required property index of the property named, which is its
 * place in the names the caller gave, the first if the name was given more
 * than once. When the caller named no property, it is the place of the
 * property in the first data set loaded.
 * @param available properties instance
 * @param propertyName string containing the property name
 * @return 0 based required property index, or -1 if the data set does not
 * have the property
 */
EXTERNAL int fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
	fiftyoneDegreesPropertiesAvailable *available,
	const char *propertyName);

/**
 * Maps the required property index to the index in the source data structure.
 * @param available properties instance
 * @param requiredPropertyIndex index of the property in the names the caller
 * gave, or when the caller named no property, in the first data set loaded
 * @return 0 based index of the property in the source, or -1 if the index is
 * outside the names given or the data set does not have the property
 */
EXTERNAL int fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
	fiftyoneDegreesPropertiesAvailable *available,
	int requiredPropertyIndex);

/**
 * Gets the name as a string from the required property index. A property the
 * data set does not have gives the name the caller gave.
 * @param available properties instance
 * @param requiredPropertyIndex index of the property in the names the caller
 * gave, or when the caller named no property, in the first data set loaded
 * @return string representation of the property, or NULL if the index is
 * outside the names given
 */
EXTERNAL fiftyoneDegreesString*
	fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
		fiftyoneDegreesPropertiesAvailable *available,
		int requiredPropertyIndex);

/**
 * The functions below are inline, so they add no name to the library.
 */
#if defined(_MSC_VER)
#define FIFTYONE_DEGREES_PROPERTIES_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define FIFTYONE_DEGREES_PROPERTIES_INLINE static __inline__
#else
#define FIFTYONE_DEGREES_PROPERTIES_INLINE static inline
#endif

/**
 * Gets the number of required property indexes, which is the number of names
 * the caller gave, or when the caller named no property, the number of
 * properties the first data set loaded has. Use it, rather than the count
 * field, with
 * #fiftyoneDegreesPropertiesGetNameFromRequiredIndex and the reads that take
 * a required property index.
 * @param available properties instance
 * @return number of required property indexes
 */
FIFTYONE_DEGREES_PROPERTIES_INLINE uint32_t
	fiftyoneDegreesPropertiesGetRequiredPropertyCount(
		const fiftyoneDegreesPropertiesAvailable *available) {
	if (available == NULL) {
		return 0;
	}
	return available->named != NULL ? available->namedCount : available->count;
}

/**
 * Gets the available index of the property at the required property index.
 * @param available properties instance
 * @param requiredPropertyIndex index of the property in the names the caller
 * gave, or when the caller named no property, in the first data set loaded
 * @return 0 based available index, or -1 if the index is outside the names
 * given or the data set does not have the property
 */
FIFTYONE_DEGREES_PROPERTIES_INLINE int fiftyoneDegreesPropertiesGetAvailableIndex(
	const fiftyoneDegreesPropertiesAvailable *available,
	int requiredPropertyIndex) {
	if (available == NULL || requiredPropertyIndex < 0) {
		return -1;
	}
	if (available->named != NULL) {
		return (uint32_t)requiredPropertyIndex < available->namedCount ?
			(int)available->named[requiredPropertyIndex].availableIndex :
			-1;
	}
	return (uint32_t)requiredPropertyIndex < available->count ?
		requiredPropertyIndex :
		-1;
}

/**
 * Gets the available index of the property named.
 * @param available properties instance
 * @param propertyName string containing the property name
 * @return 0 based available index, or -1 if the data set does not have the
 * property
 */
FIFTYONE_DEGREES_PROPERTIES_INLINE int
	fiftyoneDegreesPropertiesGetAvailableIndexFromName(
		fiftyoneDegreesPropertiesAvailable *available,
		const char *propertyName) {
	return fiftyoneDegreesPropertiesGetAvailableIndex(
		available,
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			available,
			propertyName));
}

/**
 * Maps the available index to the index in the source data structure.
 * @param available properties instance
 * @param availableIndex index of the property in the available properties
 * @return 0 based index of the property in the source, or -1 if the index is
 * outside the available properties
 */
FIFTYONE_DEGREES_PROPERTIES_INLINE int
	fiftyoneDegreesPropertiesGetPropertyIndexFromAvailableIndex(
		const fiftyoneDegreesPropertiesAvailable *available,
		int availableIndex) {
	if (available == NULL ||
		availableIndex < 0 ||
		(uint32_t)availableIndex >= available->count) {
		return -1;
	}
	return (int)available->items[availableIndex].propertyIndex;
}

/**
 * Check if the 'SetHeader' properties are included in the
 * available required properties.
 * @param available properties instance
 * @return bool whether 'SetHeader' properties are included
 */
EXTERNAL bool fiftyoneDegreesPropertiesIsSetHeaderAvailable(
	fiftyoneDegreesPropertiesAvailable* available);

/**
 * Checks whether the properties required name some properties, and fewer than
 * the data file has.
 * @param properties the caller's required properties, or NULL for all
 * @param total number of properties in the data file
 * @return true if some but not all of the properties are required
 */
EXTERNAL bool fiftyoneDegreesPropertiesRequiredNamesFewerThan(
	const fiftyoneDegreesPropertiesRequired *properties,
	uint32_t total);

/**
 * Frees the memory and resources used by the properties results previously 
 * created using the #fiftyoneDegreesPropertiesCreate method.
 * @param available properties instance to be freed
 */
EXTERNAL void fiftyoneDegreesPropertiesFree(
	fiftyoneDegreesPropertiesAvailable *available);

/**
 * @}
 */

#endif
