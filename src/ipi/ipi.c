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

#include "ipi.h"
#include "fiftyone.h"
#include "../common/config.h"
#include "constantsIpi.h"
#include "../common/collectionKeyTypes.h"
#include "graph/graph.h"

MAP_TYPE(Collection)

/**
 * GENERAL MACROS TO IMPROVE READABILITY
 */

/** Offset used for a null profile. */
#define NULL_PROFILE_OFFSET UINT32_MAX
/** Offset does not have value. */
#define NULL_VALUE_OFFSET UINT32_MAX
/** Dynamic component */
#define DYNAMIC_COMPONENT_OFFSET UINT32_MAX

/** Default value and percentage separator */
#define DEFAULT_VALUE_PERCENTAGE_SEPARATOR ":"
/** Default values separator */
#define DEFAULT_VALUES_SEPARATOR "|"

/**
 * States for the entries of the has values cache in the results. The cache
 * records the outcome of the profile walk performed to determine whether a
 * required property has values for the current results avoiding repeated
 * walks for the same property within a single detection.
 */
/** The walk has not been performed for the property. */
#define IPI_HAS_VALUES_UNKNOWN 0
/** The walk found at least one value for the property. */
#define IPI_HAS_VALUES_TRUE 1
/** The walk found no values for the property. */
#define IPI_HAS_VALUES_FALSE 2

/**
 * Bytes in the slot that keeps a copy of the profile of a result. A larger
 * profile is read again for each property, rather than allocating during a
 * lookup.
 */
#define IPI_KEPT_PROFILE_BYTES 512

#define COMPONENT(d, i) i < d->componentsList.count ? \
(Component*)d->componentsList.items[i].data.ptr : NULL

#define MAX_CONCURRENCY(t) if (config->t.concurrency > concurrency) { \
concurrency = config->t.concurrency; }

#define COLLECTION_CREATE_MEMORY(t) \
dataSet->t = CollectionCreateFromMemory( \
reader, \
dataSet->header.t); \
if (dataSet->t == NULL) { \
	return CORRUPT_DATA; \
}

#define COLLECTION_CREATE_FILE_WITH_CONFIG(t,f,c) \
dataSet->t = CollectionCreateFromFile( \
	file, \
	&dataSet->b.b.filePool, \
	c, \
	dataSet->header.t, \
	f); \
if (dataSet->t == NULL) { \
	return getCollectionFileStatus(dataSet, dataSet->header.t, 0); \
}

#define COLLECTION_CREATE_FILE(t,f) \
COLLECTION_CREATE_FILE_WITH_CONFIG(t,f,&dataSet->config.t)

/** 
 * Get min/max values with header guards to prevent redefinition warnings
 * when amalgamated with system headers (e.g., macOS sys/param.h)
 */
/** Get min value */
#ifndef MIN
#define MIN(a,b) a < b ? a : b
#endif /* MIN */
/** Get max value */
#ifndef MAX
#define MAX(a,b) a > b ? a : b
#endif /* MAX */

/**
 * PRIVATE DATA STRUCTURES
 */

/**
 * Used to pass a data set pointer and an exception to methods that require a
 * callback method and a void pointer for state used by the callback method.
 */
typedef struct state_with_exception_t {
	void* state; /* Pointer to the data set or other state information */
	Exception* exception; /* Pointer to the exception structure */
} stateWithException;

typedef struct state_with_weighting_t {
	void* subState; /* Pointer to a data set or other information */
	uint16_t rawWeighting;
} stateWithWeighting;

/**
 * Used to pass a state together with an unique header index which
 * might be used to compared against evidence.
 */
typedef struct state_with_unique_header_index_t {
	void* subState; /* Pointer to the data set or other state information */
	uint32_t headerIndex; /* An unique header index to use */
} stateWithUniqueHeaderIndex;

/**
 * Used to pass the results being populated together with the mask of
 * components whose graphs are evaluated. See COMPONENT_MASK_ENABLED.
 */
typedef struct results_with_component_mask_t {
	ResultsIpi* results; /* Results being populated */
	uint32_t componentMask; /* Components whose graphs are evaluated */
} resultsWithComponentMask;

/**
 * Used to represent the structure within a profile groups item
 */
#pragma pack(push, 2)
typedef struct profile_combination_component_index_t {
	uint16_t index; /* Index to the first profile of the component
					in the profiles list */
	uint16_t count; /* The number of profiles presents for that
					component */
} componentIndex;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct offset_percentage_t {
	uint32_t offset; /* Offset to a profiles collection item */
	uint16_t rawWeighting; /* The weight of the item in the matched IP range, out of 65535 */
} offsetPercentage;
#pragma pack(pop)

/**
 * All profile weightings in a groups should add up to exactly this number.
 */
static const uint16_t FULL_RAW_WEIGHTING = 0xFFFFU;

/**
 * PRESET IP INTELLIGENCE CONFIGURATIONS
 */

/**
 * The expected version of the data file. Version 4.6 files store profile
 * offsets, and the profiles collection length in the header, in the unit
 * declared by the header's profilesOffsetShift field, with profile records
 * aligned to unit boundaries relative to the start of the collection. The
 * 32 bit stored offsets can then address profile data beyond 4GB. All
 * property values, including IpRangeStart and IpRangeEnd, are stored in
 * profiles exactly as in version 4.5. Version 4.6 is otherwise identical
 * to 4.5, but the API and the data file move together as a single version
 * transition, so earlier versions are not supported.
 */
#define FIFTYONE_DEGREES_IPI_TARGET_VERSION_MAJOR 4
#define FIFTYONE_DEGREES_IPI_TARGET_VERSION_MINOR 6

/**
 * The largest profiles offset shift the header can declare. A shift of 8
 * gives 256 byte units addressing 1TB, well beyond any expected file, so
 * larger values are refused.
 */
#define FIFTYONE_DEGREES_IPI_MAX_PROFILES_OFFSET_SHIFT 8

#undef FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY
#define FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY true
fiftyoneDegreesConfigIpi fiftyoneDegreesIpiInMemoryConfig = {
	{FIFTYONE_DEGREES_CONFIG_DEFAULT_NO_INDEX},
	{0,0,0}, // Strings
	{0,0,0}, // Components
	{0,0,0}, // Maps
	{0,0,0}, // Properties
	{0,0,0}, // Values
	{0,0,0}, // Profiles
	{0,0,0}, // Graphs
	{0,0,0}, // ProfileGroups
	{0,0,0}, // PropertyTypes
	{0,0,0}, // ProfileOffsets
	{0,0,0}  // Graph
};
#undef FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY
#define FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY \
FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY_DEFAULT

fiftyoneDegreesConfigIpi fiftyoneDegreesIpiHighPerformanceConfig = {
	{ FIFTYONE_DEGREES_CONFIG_DEFAULT_NO_INDEX },
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Strings
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Components
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Maps
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Properties
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Values
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Profiles
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Graphs
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // ProfileGroups
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // PropertyTypes
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // ProfileOffsets
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }  // Graph
};

fiftyoneDegreesConfigIpi fiftyoneDegreesIpiLowMemoryConfig = {
	{ FIFTYONE_DEGREES_CONFIG_DEFAULT_NO_INDEX },
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Strings
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Components
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Maps
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Properties
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Values
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Profiles
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Graphs
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // ProfileGroups
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // PropertyTypes
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // ProfileOffsets
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }  // Graph
};

// The profile offsets and the property types are read for each property of every
// lookup, so the Balanced configurations hold both in memory.
#define FIFTYONE_DEGREES_IPI_CONFIG_BALANCED \
{ FIFTYONE_DEGREES_CONFIG_DEFAULT_NO_INDEX }, \
{ FIFTYONE_DEGREES_STRING_LOADED, FIFTYONE_DEGREES_STRING_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Strings */ \
{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Components */ \
{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Maps */ \
{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Properties */ \
{ FIFTYONE_DEGREES_VALUE_LOADED, FIFTYONE_DEGREES_VALUE_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Values */ \
{ FIFTYONE_DEGREES_PROFILE_LOADED, FIFTYONE_DEGREES_PROFILE_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Profiles */ \
{ FIFTYONE_DEGREES_IP_GRAPHS_LOADED, FIFTYONE_DEGREES_IP_GRAPHS_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Graphs */ \
{ FIFTYONE_DEGREES_PROFILE_GROUPS_LOADED, FIFTYONE_DEGREES_PROFILE_GROUPS_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* ProfileGroups */ \
{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* ProfileOffsets */ \
{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* PropertyTypes */ \
{ FIFTYONE_DEGREES_IP_GRAPH_LOADED, FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY } /* Graph */

fiftyoneDegreesConfigIpi fiftyoneDegreesIpiBalancedConfig = {
	FIFTYONE_DEGREES_IPI_CONFIG_BALANCED
};

fiftyoneDegreesConfigIpi fiftyoneDegreesIpiDefaultConfig = {
	FIFTYONE_DEGREES_IPI_CONFIG_BALANCED
};

#undef FIFTYONE_DEGREES_CONFIG_USE_TEMP_FILE
#define FIFTYONE_DEGREES_CONFIG_USE_TEMP_FILE true

// BalancedTemp config with reuseTempFile enabled to avoid copying the
// data file for every test. This significantly improves performance on
// Windows where file I/O is slower.
fiftyoneDegreesConfigIpi fiftyoneDegreesIpiBalancedTempConfig = {
	{
		FIFTYONE_DEGREES_CONFIG_ALL_IN_MEMORY, /* allInMemory */
		true, /* usesUpperPrefixedHeaders */
		false, /* freeData */
		true, /* useTempFile */
		true, /* reuseTempFile - ENABLED for better performance */
		NULL, /* tempDirs */
		0, /* tempDirCount */
		false /* propertyValueIndex */
	},
	{ FIFTYONE_DEGREES_STRING_LOADED, FIFTYONE_DEGREES_STRING_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Strings */
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Components */
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Maps */
	{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Properties */
	{ FIFTYONE_DEGREES_VALUE_LOADED, FIFTYONE_DEGREES_VALUE_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Values */
	{ FIFTYONE_DEGREES_PROFILE_LOADED, FIFTYONE_DEGREES_PROFILE_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Profiles */
	{ FIFTYONE_DEGREES_IP_GRAPHS_LOADED, FIFTYONE_DEGREES_IP_GRAPHS_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Graphs */
	{ FIFTYONE_DEGREES_PROFILE_GROUPS_LOADED, FIFTYONE_DEGREES_PROFILE_GROUPS_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* ProfileGroups */
	{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* ProfileOffsets */
	{ FIFTYONE_DEGREES_PROPERTY_LOADED, FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* PropertyTypes */
	{ FIFTYONE_DEGREES_IP_GRAPH_LOADED, FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY } /* Graph */
};
#undef FIFTYONE_DEGREES_CONFIG_USE_TEMP_FILE
#define FIFTYONE_DEGREES_CONFIG_USE_TEMP_FILE \
FIFTYONE_DEGREES_CONFIG_USE_TEMP_FILE_DEFAULT

/**
 * IP INTELLIGENCE METHODS
 */

static void resultIpiReset(ResultIpi* result) {
	memset(result->targetIpAddress.value, 0, FIFTYONE_DEGREES_IPV6_LENGTH);
	result->targetIpAddress.type = IP_TYPE_INVALID;
	result->componentIndex = -1;
	result->keptProfile = NULL;
}

static int compareIpAddresses(
	const byte* address1,
	const byte* address2,
	int length) {
	for (int i = 0; i < length; i++) {
		const int difference = (int)address1[i] - (int)address2[i];
		if (difference != 0) return difference;
	}
	return 0;
}

static CollectionKeyType CollectionKeyType_Ipv4Range = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_IPV4_RANGE,
	sizeof(Ipv4Range),
	NULL,
};
static CollectionKeyType CollectionKeyType_Ipv6Range = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_IPV6_RANGE,
	sizeof(Ipv6Range),
	NULL,
};

static int compareToIpv4Range(
	const void * const state,
	const Item* item,
	long curIndex,
	Exception* exception) {
	int result = 0;
	const fiftyoneDegreesIpAddress target = *((const fiftyoneDegreesIpAddress *)state);
	// We will terminate if IP address is within the range between the current item and the next item
	const int tempResult = compareIpAddresses(((Ipv4Range*)item->data.ptr)->start, target.value, FIFTYONE_DEGREES_IPV4_LENGTH);
	if (tempResult < 0) {
		Item nextItem;
		DataReset(&nextItem.data);
		if ((uint32_t)curIndex + 1 < item->collection->count) {
			const CollectionKey curKey = {
				(uint32_t)++curIndex,
				&CollectionKeyType_Ipv4Range,
			};
			if (item->collection->get(
				item->collection,
				&curKey,
				&nextItem,
				exception) != NULL && EXCEPTION_OKAY) {
				if (compareIpAddresses(
					((Ipv4Range*)nextItem.data.ptr)->start,
					target.value,
					FIFTYONE_DEGREES_IPV4_LENGTH) <= 0) {
					result = -1;
				}
				COLLECTION_RELEASE(item->collection, &nextItem);
			}
		}
	}
	else if (tempResult > 0 && curIndex > 0) {
		// The IP address is out of range
		// NOTE: If the current index is 0
		// There is no more item lower so return the current
		result = 1;
	}
	return result;
}

static int compareToIpv6Range(
	const void * const state,
	const Item * const item,
	long curIndex,
	Exception* exception) {
	int result = 0;
	const fiftyoneDegreesIpAddress target = *((fiftyoneDegreesIpAddress*)state);
	// We will terminate if IP address is within the range between the current item and the next item
	const int tempResult = compareIpAddresses(((Ipv6Range*)item->data.ptr)->start, target.value, FIFTYONE_DEGREES_IPV6_LENGTH);
	if (tempResult < 0) {
		Item nextItem;
		DataReset(&nextItem.data);
		if ((uint32_t)curIndex + 1 < item->collection->count) {
			const CollectionKey curKey = {
				(uint32_t)++curIndex,
				&CollectionKeyType_Ipv6Range,
			};
			if (item->collection->get(
				item->collection,
				&curKey,
				&nextItem,
				exception) != NULL && EXCEPTION_OKAY) {

				if (compareIpAddresses(((Ipv6Range*)nextItem.data.ptr)->start, target.value, FIFTYONE_DEGREES_IPV6_LENGTH) <= 0) {
					// The IP address is not within the range
					result = -1;
				}
				COLLECTION_RELEASE(item->collection, &nextItem);
			}
		}
	}
	else if (tempResult > 0 && curIndex > 0) {
		// The IP address is out of range
		// NOTE: There is no more item lower
		// so return the current
		result = 1;
	}
	return result;
}

static void setResultFromIpAddress(
	ResultIpi* const result,
	const DataSetIpi* const dataSet,
	byte componentId,
	Exception* const exception) {
	const fiftyoneDegreesIpiCgResult graphResult = fiftyoneDegreesIpiGraphEvaluate(
		dataSet->graphsArray,
		componentId,
		result->targetIpAddress, 
		exception);
	if (graphResult.rawOffset != NULL_PROFILE_OFFSET && EXCEPTION_OKAY) {
		result->graphResult = graphResult;
	}
}

/**
 * DATA INITIALISE AND RESET METHODS
 */

static void resetDataSet(DataSetIpi* dataSet) {
	DataSetReset(&dataSet->b.b);
	ListReset(&dataSet->componentsList);
	dataSet->componentsAvailable = NULL;
	dataSet->components = NULL;
	dataSet->maps = NULL;
	dataSet->graphs = NULL;
	dataSet->profileGroups = NULL;
	dataSet->profileOffsets = NULL;
	dataSet->profiles = NULL;
	dataSet->properties = NULL;
	dataSet->propertyTypes = NULL;
	dataSet->strings = NULL;
	dataSet->values = NULL;
	dataSet->graphsArray = NULL;
}

static void freeDataSet(void* dataSetPtr) {
	DataSetIpi* dataSet = (DataSetIpi*)dataSetPtr;

	// Free the common data set fields.
	DataSetFree(&dataSet->b.b);

	// Free the resources associated with the graphs.
	if (dataSet->graphsArray) {
		fiftyoneDegreesIpiGraphFree(dataSet->graphsArray);
	}

	// Free the memory used for the lists and collections.
	ListFree(&dataSet->componentsList);
	Free(dataSet->componentsAvailable);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->strings);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->components);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->properties);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->maps);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->values);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->profiles);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->propertyTypes);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->graphs);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->profileOffsets);
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->profileGroups);
	
	// Finally free the memory used by the resource itself as this is always
	// allocated within the IP Intelligence init manager method.
	Free(dataSet);
}

static long initGetHttpHeaderString(
	void *state,
	uint32_t index,
	Item *nameItem) {
	const DataSetIpi *dataSet =
		(DataSetIpi*)((stateWithException*)state)->state;
	Exception *exception = ((stateWithException*)state)->exception;
	uint32_t i = 0, c = 0;
	Component *component = COMPONENT(dataSet, c);
	c++;
	while (component != NULL) {
		if (index < i + component->keyValuesCount) {
			const ComponentKeyValuePair *keyValue =
				ComponentGetKeyValuePair(
					component,
					(uint16_t)(index - i),
					exception);
			nameItem->collection = NULL;
			StoredBinaryValueGet(
				dataSet->strings,
				keyValue->key,
				FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // key is string
				nameItem,
				exception);
			return keyValue->key;
		}
		i += component->keyValuesCount;
		component = COMPONENT(dataSet, c);
		c++;
	}
	return -1;
}

static const String* initGetPropertyString(
	void* state,
	uint32_t index,
	Item* item) {
	const String* name = NULL;
	Item propertyItem;
	Property* property;
	const DataSetIpi* dataSet = (DataSetIpi*)((stateWithException*)state)->state;
	Exception* exception = ((stateWithException*)state)->exception;
	const uint32_t propertiesCount = CollectionGetCount(dataSet->properties);
	DataReset(&item->data);
	if (index < propertiesCount) {
		DataReset(&propertyItem.data);
		item->collection = NULL;
		item->handle = NULL;
		const CollectionKey indexKey = {
			index,
			CollectionKeyType_Property,
		};
		property = (Property*)dataSet->properties->get(
			dataSet->properties,
			&indexKey,
			&propertyItem,
			exception);
		if (property != NULL) {
			if (EXCEPTION_OKAY) {
				name = PropertyGetName(
					dataSet->strings,
					property,
					item,
					exception);
			}
			COLLECTION_RELEASE(dataSet->properties, &propertyItem);
		}
	}
	return name;
}

static StatusCode initComponentsAvailable(
	DataSetIpi* dataSet,
	Exception* exception) {
	uint32_t i;
	Property* property;
	Item item;
	DataReset(&item.data);

	for (i = 0;
		i < dataSet->b.b.available->count;
		i++) {
		property = PropertyGet(
			dataSet->properties,
			dataSet->b.b.available->items[i].propertyIndex,
			&item,
			exception);
		if (property == NULL || EXCEPTION_FAILED) {
			return COLLECTION_FAILURE;
		}
		dataSet->componentsAvailable[property->componentIndex] = true;
		// Record the component so required property indexes can be turned
		// into a component mask without reading the property again.
		dataSet->b.b.available->items[i].componentIndex =
			property->componentIndex;
		COLLECTION_RELEASE(dataSet->properties, &item);
	}

	// Count the number of components with available properties. Needed when
	// creating results to allocate sufficient capacity for all the components.
	dataSet->componentsAvailableCount = 0;
	for (i = 0; i < dataSet->componentsList.count; i++) {
		if (dataSet->componentsAvailable[i]) {
			dataSet->componentsAvailableCount++;
		}
	}

	return SUCCESS;
}

static int findPropertyIndexByName(
	Collection *properties,
	Collection *strings,
	char *name,
	Exception *exception) {
	int index;
	bool found = false;
	Property *property;
	const String *propertyName;
	Item propertyItem, nameItem;
	const int count = CollectionGetCount(properties);
	DataReset(&propertyItem.data);
	DataReset(&nameItem.data);
	for (index = 0; index < count && found == false; index++) {
		property = PropertyGet(
			properties,
			index,
			&propertyItem,
			exception);
		if (property != NULL &&
			EXCEPTION_OKAY) {
			propertyName = PropertyGetName(
				strings,
				property,
				&nameItem,
				exception);
			if (propertyName != NULL && EXCEPTION_OKAY) {
				if (StringCompare(name, &propertyName->value) == 0) {
					found = true;
				}
				COLLECTION_RELEASE(strings, &nameItem);
			}
			COLLECTION_RELEASE(properties, &propertyItem);
		}
	}
	return found ? index : -1;
}

static void initGetEvidencePropertyRelated(
	DataSetIpi* const dataSet,
	PropertyAvailable* const availableProperty,
	EvidenceProperties* const evidenceProperties,
	int* const count,
	char* const suffix,
	Exception* const exception) {
	const Property* property;
	const String* name;
	const String* availableName = (String*)availableProperty->name.data.ptr;
	const int requiredLength = ((int)strlen(suffix)) + availableName->size - 1;
	Item propertyItem, nameItem;
	DataReset(&propertyItem.data);
	DataReset(&nameItem.data);
	const int propertiesCount = CollectionGetCount(dataSet->properties);
	for (int propertyIndex = 0; 
		propertyIndex < propertiesCount && EXCEPTION_OKAY; 
		propertyIndex++) {
		property = PropertyGet(
			dataSet->properties,
			propertyIndex,
			&propertyItem,
			exception);
		if (property != NULL && EXCEPTION_OKAY) {
			name = &StoredBinaryValueGet(
				dataSet->strings,
				property->nameOffset,
				FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // name is string
				&nameItem,
				exception)->stringValue;
			if (name != NULL && EXCEPTION_OKAY) {
				if (requiredLength == name->size -1 &&
					// Check that the available property matches the start of
					// the possible related property.
					StringCompareLength(
						&availableName->value,
						&name->value,
						(size_t)availableName->size - 1) == 0 && 
					// Check that the related property has a suffix that 
					// matches the one provided to the method.
					StringCompare(
						&name->value + availableName->size - 1, 
						suffix) == 0) {
					if (evidenceProperties != NULL) {
						evidenceProperties->items[*count] = propertyIndex;
					}
					(*count)++;
				}
				COLLECTION_RELEASE(dataSet->strings, &nameItem);
			}
			COLLECTION_RELEASE(dataSet->properties, &propertyItem);
		}
	}
}

static uint32_t initGetEvidenceProperties(
	void* state,
	fiftyoneDegreesPropertyAvailable* availableProperty,
	fiftyoneDegreesEvidenceProperties* evidenceProperties) {
	int count = 0;
	DataSetIpi* dataSet =
		(DataSetIpi*)((stateWithException*)state)->state;
	Exception* exception = ((stateWithException*)state)->exception;

	// Any properties that have a suffix of JavaScript and are associated with
	// an available property should also be added. These are used to gather
	// evidence from JavaScript that might impact the value returned.
	initGetEvidencePropertyRelated(
		dataSet,
		availableProperty,
		evidenceProperties,
		&count,
		"JavaScript",
		exception);

	return (uint32_t)count;
}

static StatusCode initPropertiesAndHeaders(
	DataSetIpi* dataSet,
	PropertiesRequired* properties,
	Exception* exception) {
	stateWithException state;
	state.state = (void*)dataSet;
	state.exception = exception;
	StatusCode status =  DataSetInitProperties(
		&dataSet->b.b,
		properties,
		&state,
		initGetPropertyString,
		initGetEvidenceProperties);
	if (status != SUCCESS) {
		return status;
	}

	status = DataSetInitHeaders(
		&dataSet->b.b,
		&state,
		initGetHttpHeaderString,
		exception);
	if (status != SUCCESS) {
		return status;
	}

	return status;
}

static StatusCode readHeaderFromMemory(
	MemoryReader* reader,
	const DataSetIpiHeader* header) {

	// MemoryAdvance below only reports an overrun once the copy has already
	// read past the end, so the space is checked here instead. The pointers
	// are compared before subtracting because a negative size puts lastByte
	// behind current.
	if (reader->current > reader->lastByte ||
		(size_t)(reader->lastByte - reader->current) <
		sizeof(DataSetIpiHeader)) {
		return CORRUPT_DATA;
	}

	// Copy the bytes that make up the dataset header.
	if (memcpy(
		(void*)header,
		(const void*)reader->current,
		sizeof(DataSetIpiHeader)) != header) {
		return CORRUPT_DATA;
	}

	// Move the current pointer to the next data structure.
	return MemoryAdvance(reader, sizeof(DataSetIpiHeader)) == true ?
		SUCCESS : CORRUPT_DATA;
}

static StatusCode checkVersion(DataSetIpi* dataSet) {
	if (!(dataSet->header.versionMajor ==
		FIFTYONE_DEGREES_IPI_TARGET_VERSION_MAJOR &&
		dataSet->header.versionMinor ==
		FIFTYONE_DEGREES_IPI_TARGET_VERSION_MINOR)) {
		return INCORRECT_VERSION;
	}

	// The shift used to convert stored profile offsets to byte positions is
	// declared in the header and validated rather than assumed. A shift
	// outside the range is one this API cannot use, so it is reported as an
	// unsupported version. The message for corrupt data would send the reader
	// to check the file is uncompressed, which it is.
	if (dataSet->header.profilesOffsetShift < 0 ||
		dataSet->header.profilesOffsetShift >
		FIFTYONE_DEGREES_IPI_MAX_PROFILES_OFFSET_SHIFT) {
		return INCORRECT_VERSION;
	}
#ifndef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
	// Converting shifted offsets to byte positions requires the 64 bit
	// arithmetic of large data file support. A shift of zero passing this
	// check does not mean the file loads, because without that support the
	// collection positions in the header are read as 4 bytes where every IP
	// Intelligence data file has 8. The builds in this repository always
	// define it for IP Intelligence.
	if (dataSet->header.profilesOffsetShift != 0) {
		return INCORRECT_VERSION;
	}
#endif
	return SUCCESS;
}

/**
 * @return the number of bits to shift a stored profile offset left to
 * convert it to a byte position within the profiles collection. Declared by
 * the data file in the header, and zero where profile offsets are byte
 * positions.
 */
#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
static byte getProfilesOffsetShift(const DataSetIpi* dataSet) {
	return (byte)dataSet->header.profilesOffsetShift;
}
#endif

/**
 * Gets the status for a failure, which is the exception's if it holds one. An
 * exception holding success was not cleared before the call, which stops the
 * init methods in the same way.
 */
static StatusCode getFailureStatus(
	StatusCode status,
	Exception *exception) {
#ifdef FIFTYONE_DEGREES_EXCEPTIONS_DISABLED
	(void)exception;
#else
	if (EXCEPTION_FAILED) {
		return exception->status == SUCCESS ? INVALID_INPUT : exception->status;
	}
#endif
	return status;
}

static StatusCode initDataSetPost(
	DataSetIpi* dataSet,
	Exception* exception) {
	
	// Initialise the components lists. A list short of the count fails, which
	// the status shows whether or not there is an exception to show it.
	ComponentInitList(
		dataSet->components,
		&dataSet->componentsList,
		dataSet->header.components.count,
		exception);
	if (EXCEPTION_FAILED ||
		dataSet->componentsList.count < dataSet->header.components.count) {
		return getFailureStatus(
			dataSet->componentsList.items == NULL ?
				INSUFFICIENT_MEMORY : CORRUPT_DATA,
			exception);
	}

	// Initialise the components which have required properties.
	dataSet->componentsAvailable = Malloc(
		sizeof(bool) * dataSet->componentsList.count);
	if (dataSet->componentsAvailable == NULL) {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
		return INSUFFICIENT_MEMORY;
	}
	memset(
		(char*)dataSet->componentsAvailable,
		0,
		sizeof(bool) * dataSet->componentsList.count);
	return SUCCESS;
}

static StatusCode initWithMemory(
	DataSetIpi* dataSet,
	MemoryReader* reader,
	Exception* exception) {
	StatusCode status = SUCCESS;

	// Indicate that the data is in memory and there is no connection to the
	// source data file.
	dataSet->b.b.isInMemory = true;

	// Check that the reader is configured correctly.
	if (reader->current == NULL) {
		return NULL_POINTER;
	}

	// Copy the bytes that form the header from the start of the memory
	// location to the data set data.ptr provided.
	status = readHeaderFromMemory(reader, &dataSet->header);
	if (status != SUCCESS) {
		return status;
	}

	// Check the version.
	status = checkVersion(dataSet);
	if (status != SUCCESS) {
		return status;
	}

	// Create each of the collections.
	const uint32_t stringsCount = dataSet->header.strings.count;
	*(uint32_t*)(&dataSet->header.strings.count) = 0;
	COLLECTION_CREATE_MEMORY(strings)
	*(uint32_t*)(&dataSet->header.strings.count) = stringsCount;

	// Override the header count so that the variable collection can work.
	const uint32_t componentCount = dataSet->header.components.count;
	*(uint32_t*)(&dataSet->header.components.count) = 0;
	COLLECTION_CREATE_MEMORY(components)
	*(uint32_t*)(&dataSet->header.components.count) = componentCount;

	COLLECTION_CREATE_MEMORY(maps)
	COLLECTION_CREATE_MEMORY(properties)
	COLLECTION_CREATE_MEMORY(values)

	const uint32_t profileCount = dataSet->header.profiles.count;
	*(uint32_t*)(&dataSet->header.profiles.count) = 0;
#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
	// The profiles collection stores offsets in the unit declared by the
	// header. No other collection uses shifted offsets.
	dataSet->profiles = fiftyoneDegreesCollectionCreateFromMemoryWithOffsetShift(
		reader,
		dataSet->header.profiles,
		getProfilesOffsetShift(dataSet));
	if (dataSet->profiles == NULL) {
		return CORRUPT_DATA;
	}
#else
	COLLECTION_CREATE_MEMORY(profiles)
#endif
	*(uint32_t*)(&dataSet->header.profiles.count) = profileCount;

	COLLECTION_CREATE_MEMORY(graphs);

	COLLECTION_CREATE_MEMORY(profileGroups);
	COLLECTION_CREATE_MEMORY(propertyTypes);
	COLLECTION_CREATE_MEMORY(profileOffsets);

	dataSet->graphsArray = fiftyoneDegreesIpiGraphCreateFromMemory(
		dataSet->graphs,
		reader,
		exception);

	/* Check that the current pointer equals the last byte */
	if (reader->lastByte != reader->current) {
		return POINTER_OUT_OF_BOUNDS;
	}

	return initDataSetPost(dataSet, exception);
}

static StatusCode initInMemory(
	DataSetIpi* dataSet,
	Exception* exception) {
	MemoryReader reader;

	// The data set is freed by the caller on failure rather than here, so
	// that the caller can still read the file name from it to delete a temp
	// file, and so the file path frees it too.
	//
	// Read the data from the source file into memory using the reader to
	// store the pointer to the first and last bytes.
	StatusCode status = DataSetInitInMemory(
		&dataSet->b.b,
		&reader);
	if (status != SUCCESS) {
		return status;
	}

	// Use the memory reader to initialize the IP Intelligence data set.
	return initWithMemory(dataSet, &reader, exception);
}

static void initDataSet(DataSetIpi* dataSet, ConfigIpi** config) {
	// If no config has been provided then use the balanced configuration.
	if (*config == NULL) {
		*config = &IpiBalancedConfig;
	}

	// Reset the data set so that if a partial initialise occurs some memory
	// can freed.
	resetDataSet(dataSet);

	// Copy the configuration into the data set to ensure it's always
	// available in cases where the source configuration gets freed.
	memcpy((void*)&dataSet->config, *config, sizeof(ConfigIpi));
	dataSet->b.b.config = &dataSet->config;
}

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY

/**
 * Gets the status for a collection that could not be created from the file,
 * as the create methods do not say why they failed. A collection the header
 * places beyond the end of the file is a fault in the file, which no
 * configuration can cause or cure. Anything else comes from the configuration
 * or a lack of memory.
 */
static StatusCode getCollectionFileStatus(
	const DataSetIpi* dataSet,
	const CollectionHeader header,
	const byte offsetShift) {
	const uint64_t end = (uint64_t)header.startPosition +
		((uint64_t)header.length << offsetShift);
	return end > (uint64_t)dataSet->b.b.filePool.length ?
		CORRUPT_DATA : INVALID_COLLECTION_CONFIG;
}

static StatusCode readHeaderFromFile(
	FILE* file,
	const DataSetIpiHeader* header) {
	// Read the bytes that make up the dataset header.
	if (fread(
		(void*)header,
		sizeof(DataSetIpiHeader),
		1,
		file) != 1) {
		return CORRUPT_DATA;
	}

	return SUCCESS;
}

/**
 * The number of bits to shift a stored profile offset by to turn it into bytes.
 */
static byte getProfilesFileShift(const DataSetIpi* dataSet) {
#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
	return getProfilesOffsetShift(dataSet);
#else
	(void)dataSet;
	return 0;
#endif
}

static StatusCode createProfilesFromFile(
	DataSetIpi* dataSet,
	FILE* file,
	const fiftyoneDegreesCollectionConfig* config) {
	const uint32_t profileCount = dataSet->header.profiles.count;
	*(uint32_t*)(&dataSet->header.profiles.count) = 0;
#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
	// The profiles collection stores offsets in the unit declared by the
	// header. No other collection uses shifted offsets.
	dataSet->profiles = fiftyoneDegreesCollectionCreateFromFileWithOffsetShift(
		file,
		&dataSet->b.b.filePool,
		config,
		dataSet->header.profiles,
		fiftyoneDegreesProfileReadFromFile,
		getProfilesOffsetShift(dataSet));
#else
	dataSet->profiles = CollectionCreateFromFile(
		file,
		&dataSet->b.b.filePool,
		config,
		dataSet->header.profiles,
		fiftyoneDegreesProfileReadFromFile);
#endif
	*(uint32_t*)(&dataSet->header.profiles.count) = profileCount;
	if (dataSet->profiles == NULL) {
		return getCollectionFileStatus(
			dataSet,
			dataSet->header.profiles,
			getProfilesFileShift(dataSet));
	}
	return SUCCESS;
}

/**
 * HOLDING WHAT THE REQUIRED PROPERTIES READ
 *
 * With the Balanced preset, a caller that names its required properties only
 * reads part of the data file. The parts it does not read stay on file, and
 * the memory that frees pays for holding more of the parts it does read, so
 * memory never rises and lookups need fewer locks, file reads and allocations.
 * Everything still reads from the file if it is not held, so this can only
 * change where an answer comes from and never the answer.
 */

/**
 * The fewest bytes between two required strings worth leaving on file, as more
 * ranges cost time on every string read. The scan for required strings records
 * parts of this size, because a gap this long always crosses the edge of a part.
 */
#define HELD_STRINGS_GAP_BYTES ((uint32_t)65536)

/** Bytes read from the file at a time when scanning records. */
#define HELD_SCAN_BUFFER_BYTES ((uint32_t)65536)

/** Bytes kept back from the budget for the memory used to keep track. */
#define HELD_RESERVED_BYTES ((uint64_t)131072)

/**
 * The most entries the profile cache is enlarged to. When measured, ten times
 * this gave little on Linux and was usually slower on Windows.
 */
#define HELD_PROFILE_CACHE_MAX_ENTRIES ((uint32_t)100000)

/**
 * The least of a run of profiles worth holding when not all of it fits, as a
 * percentage of the run.
 */
#define HELD_PROFILE_RUN_MIN_PERCENT 50

/** Bytes the allocator uses for each block a cache entry allocates. */
#define HELD_ALLOCATION_OVERHEAD_BYTES ((uint64_t)32)

/** The number of values a component id, which is a byte, can have. */
#define HELD_COMPONENT_IDS 256

/**
 * A stretch of a collection held in memory, in the units of the collection's
 * keys.
 */
typedef struct held_range_t {
	uint32_t first; /* Key of the first unit of the range */
	uint32_t end; /* Key after the last unit of the range */
	byte* memory; /* The bytes of the range */
} heldRange;

/**
 * State of a collection that holds stretches of another collection in memory
 * and passes every other read to that collection.
 */
typedef struct held_collection_t {
	Collection* source; /* Reads whatever is not held */
	uint64_t bytes; /* Bytes held in the ranges */
	uint32_t unitBytes; /* Bytes in each unit of a key */
	uint32_t count; /* Number of ranges */
	heldRange* ranges; /* The ranges in the order of their keys */
} heldCollection;

/**
 * Checks all of an item is in the range. An item of variable length that starts
 * in a range might end beyond it, and is then read from the source.
 */
static bool heldRangeHasItem(
	const Collection* collection,
	const heldCollection* state,
	const heldRange* range,
	const CollectionKey* key,
	const byte* ptr,
	Exception* exception) {
	if (collection->elementSize > 0) {
		return true;
	}
	const uint64_t available =
		(uint64_t)(range->end - key->indexOrOffset.offset) * state->unitBytes;
	if (key->keyType->initialBytesCount > available) {
		return false;
	}
	return key->keyType->getFinalSizeMethod == NULL ||
		key->keyType->getFinalSizeMethod(ptr, exception) <= available;
}

static void* heldGet(
	const Collection* collection,
	const CollectionKey* const key,
	Item* item,
	Exception* exception) {
	const heldCollection* const state =
		(const heldCollection*)collection->state;
	const uint32_t position = key->indexOrOffset.offset;
	uint32_t low = 0;
	uint32_t high = state->count;
	while (low < high) {
		const uint32_t middle = low + (high - low) / 2;
		const heldRange* const range = &state->ranges[middle];
		if (position < range->first) {
			high = middle;
		}
		else if (position >= range->end) {
			low = middle + 1;
		}
		else {
			byte* const ptr = range->memory +
				(size_t)((uint64_t)(position - range->first) * state->unitBytes);
			if (heldRangeHasItem(
				collection,
				state,
				range,
				key,
				ptr,
				exception)) {
				item->data.ptr = ptr;
				item->handle = NULL;
				item->collection = collection;
				return ptr;
			}
			break;
		}
	}
	void* const ptr = state->source->get(state->source, key, item, exception);
	if (item->collection != NULL) {
		item->collection = collection;
	}
	return ptr;
}

static void heldRelease(Item* item) {
	if (item->handle != NULL) {
		const heldCollection* const state =
			(const heldCollection*)item->collection->state;
		state->source->release(item);
	}
	else {
		DataReset(&item->data);
	}
}

static void heldFree(Collection* collection) {
	heldCollection* const state = (heldCollection*)collection->state;
	for (uint32_t i = 0; i < state->count; i++) {
		Free(state->ranges[i].memory);
	}
	FIFTYONE_DEGREES_COLLECTION_FREE(state->source);
	Free(state);
	Free(collection);
}

static void heldRangeAdd(
	heldRange* ranges,
	uint32_t* count,
	const uint32_t first,
	const uint32_t end) {
	if (end > first) {
		ranges[*count].first = first;
		ranges[*count].end = end;
		ranges[*count].memory = NULL;
		(*count)++;
	}
}

static int heldRangeCompareFirst(const void* a, const void* b) {
	const uint32_t x = ((const heldRange*)a)->first;
	const uint32_t y = ((const heldRange*)b)->first;
	return x < y ? -1 : (x > y ? 1 : 0);
}

static int heldRangeCompareLength(const void* a, const void* b) {
	const uint32_t x = ((const heldRange*)a)->end - ((const heldRange*)a)->first;
	const uint32_t y = ((const heldRange*)b)->end - ((const heldRange*)b)->first;
	return x < y ? -1 : (x > y ? 1 : 0);
}

/**
 * Replaces the collection with one holding the ranges in memory, which reads
 * everything else from the collection replaced. The ranges are put in order
 * and joined where they meet. Anything allocated is freed with the data set if
 * this fails.
 */
static StatusCode heldCreate(
	Collection** collection,
	FILE* file,
	const uint64_t startPosition,
	const uint32_t unitBytes,
	heldRange* ranges,
	const uint32_t rangeCount) {
	uint32_t count = 0;
	if (rangeCount == 0) {
		return SUCCESS;
	}
	qsort(ranges, rangeCount, sizeof(heldRange), heldRangeCompareFirst);
	for (uint32_t i = 0; i < rangeCount; i++) {
		if (count > 0 && ranges[i].first <= ranges[count - 1].end) {
			if (ranges[i].end > ranges[count - 1].end) {
				ranges[count - 1].end = ranges[i].end;
			}
		}
		else {
			ranges[count++] = ranges[i];
		}
	}

	heldCollection* const state = (heldCollection*)Malloc(
		sizeof(heldCollection) + count * sizeof(heldRange));
	if (state == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	Collection* const held = (Collection*)Malloc(sizeof(Collection));
	if (held == NULL) {
		Free(state);
		return INSUFFICIENT_MEMORY;
	}
	state->source = *collection;
	state->bytes = 0;
	state->unitBytes = unitBytes;
	state->count = 0;
	state->ranges = (heldRange*)(state + 1);
	*held = **collection;
	held->state = state;
	held->get = heldGet;
	held->release = heldRelease;
	held->freeCollection = heldFree;
	held->typeName = "CollectionHeld";
	*collection = held;

	for (uint32_t i = 0; i < count; i++) {
		const uint64_t bytes =
			(uint64_t)(ranges[i].end - ranges[i].first) * unitBytes;
		if (bytes > (uint64_t)SIZE_MAX) {
			return INSUFFICIENT_MEMORY;
		}
		byte* const memory = (byte*)Malloc((size_t)bytes);
		if (memory == NULL) {
			return INSUFFICIENT_MEMORY;
		}
		state->ranges[i] = ranges[i];
		state->ranges[i].memory = memory;
		state->count++;
		state->bytes += bytes;
		const uint64_t position =
			startPosition + (uint64_t)ranges[i].first * unitBytes;
		if (FileSeek(file, (FileOffset)position, SEEK_SET) != 0) {
			return COLLECTION_FILE_SEEK_FAIL;
		}
		if (fread(memory, 1, (size_t)bytes, file) != (size_t)bytes) {
			return COLLECTION_FILE_READ_FAIL;
		}
	}
	return SUCCESS;
}

/**
 * Calls the method for each fixed size record in a stretch of the data file.
 */
static StatusCode heldScan(
	FILE* file,
	byte* buffer,
	const uint64_t position,
	uint32_t count,
	const uint32_t recordBytes,
	void(*method)(void* state, const byte* record),
	void* state) {
	if (count == 0) {
		return SUCCESS;
	}
	if (FileSeek(file, (FileOffset)position, SEEK_SET) != 0) {
		return COLLECTION_FILE_SEEK_FAIL;
	}
	const uint32_t recordsInBuffer = HELD_SCAN_BUFFER_BYTES / recordBytes;
	while (count > 0) {
		const uint32_t next = count < recordsInBuffer ? count : recordsInBuffer;
		if (fread(buffer, recordBytes, next, file) != next) {
			return COLLECTION_FILE_READ_FAIL;
		}
		for (uint32_t i = 0; i < next; i++) {
			method(state, buffer + (size_t)i * recordBytes);
		}
		count -= next;
	}
	return SUCCESS;
}

/**
 * The lowest and highest required string offsets in part of the strings, and
 * how the highest is stored so that its size can be worked out.
 */
typedef struct held_strings_bucket_t {
	uint32_t first;
	uint32_t last;
	PropertyValueType lastType;
} heldStringsBucket;

/**
 * What is worked out before anything is held, so that nothing is allocated
 * until it is known that everything fits.
 */
typedef struct held_plan_t {
	byte* buffer; /* Used to scan records */
	heldStringsBucket* buckets; /* The strings the required properties read */
	uint32_t bucketCount; /* Number of buckets */
	uint32_t stringsBytes; /* Bytes in the strings collection */
	PropertyValueType scanType; /* How the strings being scanned are stored */
	heldRange* strings; /* The stretches of the strings to hold */
	uint32_t stringsCount; /* Number of stretches of the strings */
	heldRange* values; /* The stretches of the values to hold */
	uint32_t valuesCount; /* Number of stretches of the values */
	heldRange* profiles; /* The stretches of the profiles to hold */
	uint32_t profilesCount; /* Number of stretches of the profiles */
	bool holdProfileGroups; /* True to hold all the profile groups */
	uint64_t budget; /* Bytes that can still be held */
	uint32_t profileCacheCapacity; /* Entries for the profile cache */
} heldPlan;

static void heldFreeIfSet(void* memory) {
	if (memory != NULL) {
		Free(memory);
	}
}

static void heldPlanFree(heldPlan* plan) {
	heldFreeIfSet(plan->buffer);
	heldFreeIfSet(plan->buckets);
	heldFreeIfSet(plan->strings);
	heldFreeIfSet(plan->values);
	heldFreeIfSet(plan->profiles);
}

static void heldPlanScanValue(void* state, const byte* record) {
	heldPlan* const plan = (heldPlan*)state;
	const uint32_t offset = (uint32_t)((const Value*)record)->nameOffset;
	if (offset < plan->stringsBytes) {
		heldStringsBucket* const bucket =
			&plan->buckets[offset / HELD_STRINGS_GAP_BYTES];
		if (offset < bucket->first) {
			bucket->first = offset;
		}
		if (offset >= bucket->last) {
			bucket->last = offset;
			bucket->lastType = plan->scanType;
		}
	}
}

/**
 * Creates the graphs. Those of a component with no required property read from
 * the file, which adds the memory they would have used to the budget.
 */
static StatusCode heldGraphsCreate(
	DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	bool requiredIds[HELD_COMPONENT_IDS];
	memset(requiredIds, 0, sizeof(requiredIds));
	for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
		const Component* const component = COMPONENT(dataSet, c);
		if (component != NULL && dataSet->componentsAvailable[c]) {
			requiredIds[component->componentId] = true;
		}
	}
	dataSet->graphsArray = fiftyoneDegreesIpiGraphCreateFromFileForComponents(
		dataSet->graphs,
		file,
		&dataSet->b.b.filePool,
		dataSet->config.graph,
		requiredIds,
		exception);
	if (dataSet->graphsArray == NULL) {
		return EXCEPTION_OKAY ? CORRUPT_DATA : SUCCESS;
	}
	for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
		const fiftyoneDegreesIpiCgInfo* const info =
			&dataSet->graphsArray->items[g].info;
		if (requiredIds[info->componentId] == false) {
			plan->budget += (uint64_t)info->nodes.collection.length +
				info->spans.length +
				info->spanBytes.length +
				info->clusters.length;
		}
	}
	return SUCCESS;
}

/**
 * Records the strings of a stretch of values and offers it for holding.
 */
static StatusCode heldPlanValues(
	const DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	const uint32_t first,
	const uint32_t count) {
	const uint32_t valueBytes = dataSet->values->elementSize;
	heldRangeAdd(plan->values, &plan->valuesCount, first, first + count);
	return heldScan(
		file,
		plan->buffer,
		(uint64_t)dataSet->header.values.startPosition +
			(uint64_t)first * valueBytes,
		count,
		valueBytes,
		heldPlanScanValue,
		plan);
}

/**
 * Plans for the values of each required property. A default value is only
 * offered on its own if it is not in the range of its property.
 */
static StatusCode heldPlanProperties(
	DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	const PropertiesAvailable* const available = dataSet->b.b.available;
	const uint32_t valueCount = dataSet->values->count;
	plan->values = (heldRange*)Malloc(
		sizeof(heldRange) * available->count * 2);
	if (plan->values == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	for (uint32_t i = 0; i < available->count; i++) {
		Item item;
		DataReset(&item.data);
		const Property* const property = PropertyGet(
			dataSet->properties,
			available->items[i].propertyIndex,
			&item,
			exception);
		if (property == NULL || EXCEPTION_FAILED) {
			return COLLECTION_FAILURE;
		}
		const uint32_t first = property->firstValueIndex;
		const uint32_t last = property->lastValueIndex;
		const uint32_t defaultIndex = property->defaultValueIndex;
		COLLECTION_RELEASE(dataSet->properties, &item);
		plan->scanType = PropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			available->items[i].propertyIndex,
			exception);
		if (EXCEPTION_FAILED) {
			return COLLECTION_FAILURE;
		}
		StatusCode status = SUCCESS;
		if (first <= last && last < valueCount) {
			status = heldPlanValues(
				dataSet,
				file,
				plan,
				first,
				last - first + 1);
		}
		if (status == SUCCESS &&
			defaultIndex < valueCount &&
			(defaultIndex < first || defaultIndex > last)) {
			status = heldPlanValues(dataSet, file, plan, defaultIndex, 1);
		}
		if (status != SUCCESS) {
			return status;
		}
	}
	return SUCCESS;
}

/**
 * Gets the offset after the string at the offset provided.
 */
static uint32_t heldStringEnd(
	const DataSetIpi* dataSet,
	const uint32_t offset,
	const PropertyValueType type,
	Exception* exception) {
	Item item;
	uint32_t end = offset;
	DataReset(&item.data);
	if (StoredBinaryValueGet(
		dataSet->strings,
		offset,
		type,
		&item,
		exception) != NULL && EXCEPTION_OKAY) {
		end += item.data.used;
		COLLECTION_RELEASE(dataSet->strings, &item);
	}
	return end;
}

/**
 * Works out the stretches of the strings to hold from the strings recorded,
 * which leaves on file any stretch long enough that holds no required string.
 * What that frees is added to the budget.
 */
static StatusCode heldPlanStrings(
	const DataSetIpi* dataSet,
	heldPlan* plan,
	Exception* exception) {
	const uint32_t size = plan->stringsBytes;
	uint32_t start = 0;
	uint32_t last = 0;
	PropertyValueType lastType = FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	bool found = false;
	plan->strings = (heldRange*)Malloc(
		sizeof(heldRange) * ((size / HELD_STRINGS_GAP_BYTES) + 2));
	if (plan->strings == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	for (uint32_t i = 0; i < plan->bucketCount; i++) {
		const heldStringsBucket* const bucket = &plan->buckets[i];
		if (bucket->first == UINT32_MAX) {
			continue;
		}
		if (found == false) {
			if (bucket->first >= HELD_STRINGS_GAP_BYTES) {
				start = bucket->first;
			}
			found = true;
		}
		else if (bucket->first - last >= HELD_STRINGS_GAP_BYTES) {
			const uint32_t end = heldStringEnd(
				dataSet,
				last,
				lastType,
				exception);
			if (EXCEPTION_FAILED) {
				return COLLECTION_FAILURE;
			}
			if (bucket->first > end &&
				bucket->first - end >= HELD_STRINGS_GAP_BYTES) {
				heldRangeAdd(plan->strings, &plan->stringsCount, start, end);
				start = bucket->first;
			}
		}
		last = bucket->last;
		lastType = bucket->lastType;
	}
	if (found) {
		uint32_t end = heldStringEnd(dataSet, last, lastType, exception);
		if (EXCEPTION_FAILED) {
			return COLLECTION_FAILURE;
		}
		if (end > size || size - end < HELD_STRINGS_GAP_BYTES) {
			end = size;
		}
		heldRangeAdd(plan->strings, &plan->stringsCount, start, end);
	}
	uint64_t held = 0;
	for (uint32_t i = 0; i < plan->stringsCount; i++) {
		held += plan->strings[i].end - plan->strings[i].first;
	}
	plan->budget += size - held;
	return SUCCESS;
}

/**
 * Keeps as many of the ranges as the budget allows, smallest first. A larger
 * range cannot fit once a smaller one has not.
 */
static uint32_t heldPlanSpend(
	heldPlan* plan,
	heldRange* ranges,
	const uint32_t count,
	const uint32_t unitBytes) {
	uint32_t kept = 0;
	qsort(ranges, count, sizeof(heldRange), heldRangeCompareLength);
	while (kept < count) {
		const uint64_t bytes =
			(uint64_t)(ranges[kept].end - ranges[kept].first) * unitBytes;
		if (bytes > plan->budget) {
			break;
		}
		plan->budget -= bytes;
		kept++;
	}
	return kept;
}

/** The profiles of a component, which are next to each other in the file. */
typedef struct held_profile_run_t {
	uint32_t first; /* Offset of the first profile */
	uint32_t last; /* Offset of the last profile */
} heldProfileRun;

static void heldProfileRunAdd(heldProfileRun* run, const uint32_t offset) {
	if (offset != NULL_PROFILE_OFFSET) {
		if (offset < run->first) {
			run->first = offset;
		}
		if (offset > run->last) {
			run->last = offset;
		}
	}
}

static void heldProfileRunScanOffset(void* state, const byte* record) {
	uint32_t offset;
	memcpy(&offset, record, sizeof(offset));
	heldProfileRunAdd((heldProfileRun*)state, offset);
}

static void heldProfileRunScanGroup(void* state, const byte* record) {
	heldProfileRunAdd(
		(heldProfileRun*)state,
		((const offsetPercentage*)record)->offset);
}

/**
 * Gets the number of offset units the profile at the offset provided takes up.
 */
static StatusCode heldProfileUnits(
	const DataSetIpi* dataSet,
	FILE* file,
	const uint32_t offset,
	uint32_t* units,
	Exception* exception) {
	const byte shift = getProfilesFileShift(dataSet);
	byte initial[sizeof(Profile)];
	const uint64_t position = (uint64_t)dataSet->header.profiles.startPosition +
		((uint64_t)offset << shift);
	if (FileSeek(file, (FileOffset)position, SEEK_SET) != 0) {
		return COLLECTION_FILE_SEEK_FAIL;
	}
	if (fread(initial, sizeof(initial), 1, file) != 1) {
		return COLLECTION_FILE_READ_FAIL;
	}
	const uint64_t bytes = fiftyoneDegreesProfileGetFinalSize(initial, exception);
	*units = (uint32_t)((bytes + (((uint64_t)1 << shift) - 1)) >> shift);
	return SUCCESS;
}

/**
 * Plans for the required profiles that did not fit. The profile cache grows for
 * them, and then the next run is held in part if enough of it is left. A profile
 * that runs past the end of the part reads from the cache.
 */
static void heldPlanProfilesOnFile(
	const DataSetIpi* dataSet,
	heldPlan* plan,
	heldRange* next,
	const uint64_t averageProfileBytes) {
	const byte shift = getProfilesFileShift(dataSet);
	const uint32_t capacity = dataSet->config.profiles.capacity;
	if (capacity > 0) {
		// The data of an entry grows to the largest profile it has held.
		const uint64_t entryBytes = sizeof(CacheNode) +
			HELD_ALLOCATION_OVERHEAD_BYTES + 2 * averageProfileBytes;
		const uint64_t entries = plan->budget / entryBytes;
		plan->profileCacheCapacity = entries > HELD_PROFILE_CACHE_MAX_ENTRIES ?
			HELD_PROFILE_CACHE_MAX_ENTRIES : (uint32_t)entries;
		if (plan->profileCacheCapacity > capacity) {
			plan->budget -= plan->profileCacheCapacity * entryBytes;
		}
	}
	const uint64_t units = plan->budget >> shift;
	const uint64_t runUnits = next->end - next->first;
	if (units < runUnits && units * 100 >= runUnits * HELD_PROFILE_RUN_MIN_PERCENT) {
		next->end = next->first + (uint32_t)units;
		plan->budget -= units << shift;
		plan->profilesCount++;
	}
}

/**
 * Offers the run of profiles of each component with a required property for
 * holding, smallest first.
 */
static StatusCode heldPlanProfiles(
	const DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	if (plan->budget == 0) {
		return SUCCESS;
	}
	const uint32_t size = dataSet->header.profiles.length;
	const byte shift = getProfilesFileShift(dataSet);
	const uint64_t offsetsStart =
		(uint64_t)dataSet->header.profileOffsets.startPosition;
	const uint64_t groupsStart =
		(uint64_t)dataSet->header.profileGroups.startPosition;
	uint64_t totalBytes = 0;
	uint64_t totalProfiles = 0;
	uint32_t count = 0;
	plan->profiles = (heldRange*)Malloc(
		sizeof(heldRange) * dataSet->componentsList.count);
	if (plan->profiles == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
		const Component* const component = COMPONENT(dataSet, c);
		if (component == NULL || dataSet->componentsAvailable[c] == false) {
			continue;
		}
		heldProfileRun run = { UINT32_MAX, 0 };
		uint32_t profiles = 0;
		for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
			const fiftyoneDegreesIpiCgInfo* const info =
				&dataSet->graphsArray->items[g].info;
			if (info->componentId != component->componentId) {
				continue;
			}
			// The graphs of each IP version share the profiles of a component.
			if (info->profileCount > profiles) {
				profiles = info->profileCount;
			}
			StatusCode status = heldScan(
				file,
				plan->buffer,
				offsetsStart +
					(uint64_t)info->firstProfileIndex * sizeof(uint32_t),
				info->profileCount,
				sizeof(uint32_t),
				heldProfileRunScanOffset,
				&run);
			if (status == SUCCESS) {
				status = heldScan(
					file,
					plan->buffer,
					groupsStart + (uint64_t)info->firstProfileGroupIndex *
						sizeof(offsetPercentage),
					info->profileGroupCount,
					sizeof(offsetPercentage),
					heldProfileRunScanGroup,
					&run);
			}
			if (status != SUCCESS) {
				return status;
			}
		}
		if (run.first > run.last || run.last >= size) {
			continue;
		}
		uint32_t units = 0;
		const StatusCode status = heldProfileUnits(
			dataSet,
			file,
			run.last,
			&units,
			exception);
		if (status != SUCCESS || EXCEPTION_FAILED) {
			return status;
		}
		if (units <= size - run.last) {
			heldRangeAdd(plan->profiles, &count, run.first, run.last + units);
			totalBytes += (uint64_t)(run.last + units - run.first) << shift;
			totalProfiles += profiles;
		}
	}
	plan->profilesCount = heldPlanSpend(
		plan,
		plan->profiles,
		count,
		(uint32_t)1 << shift);
	if (plan->profilesCount < count) {
		heldPlanProfilesOnFile(
			dataSet,
			plan,
			&plan->profiles[plan->profilesCount],
			totalProfiles > 0 ? totalBytes / totalProfiles : 0);
	}
	return SUCCESS;
}

/**
 * Works out what to hold. Nothing that is going to be held is allocated here.
 */
static StatusCode heldPlanCreate(
	DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	// A collection placed beyond the end of the file is a fault in the file,
	// and the scans below would otherwise report it as a failed read.
	if (getCollectionFileStatus(dataSet, dataSet->header.values, 0) ==
			CORRUPT_DATA ||
		getCollectionFileStatus(dataSet, dataSet->header.profileGroups, 0) ==
			CORRUPT_DATA ||
		getCollectionFileStatus(
			dataSet,
			dataSet->header.profiles,
			getProfilesFileShift(dataSet)) == CORRUPT_DATA) {
		return CORRUPT_DATA;
	}
	StatusCode status = heldGraphsCreate(dataSet, file, plan, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	plan->buffer = (byte*)Malloc(HELD_SCAN_BUFFER_BYTES);
	plan->stringsBytes = dataSet->header.strings.length;
	plan->bucketCount = (plan->stringsBytes / HELD_STRINGS_GAP_BYTES) + 1;
	plan->buckets = (heldStringsBucket*)Malloc(
		sizeof(heldStringsBucket) * plan->bucketCount);
	if (plan->buffer == NULL || plan->buckets == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	for (uint32_t i = 0; i < plan->bucketCount; i++) {
		plan->buckets[i].first = UINT32_MAX;
		plan->buckets[i].last = 0;
		plan->buckets[i].lastType = FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	}
	status = heldPlanProperties(dataSet, file, plan, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	status = heldPlanStrings(dataSet, plan, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	plan->budget = plan->budget > HELD_RESERVED_BYTES ?
		plan->budget - HELD_RESERVED_BYTES : 0;
	plan->valuesCount = heldPlanSpend(
		plan,
		plan->values,
		plan->valuesCount,
		dataSet->values->elementSize);

	// Holding all the profile groups frees their cache, which is usually much
	// larger than they are.
	const uint64_t groupsBytes = dataSet->header.profileGroups.length;
	const uint64_t groupsCacheBytes =
		(uint64_t)dataSet->config.profileGroups.capacity * sizeof(CacheNode);
	if (dataSet->config.profileGroups.loaded == false &&
		groupsBytes <= plan->budget + groupsCacheBytes) {
		plan->holdProfileGroups = true;
		plan->budget = plan->budget + groupsCacheBytes - groupsBytes;
	}
	return heldPlanProfiles(dataSet, file, plan, exception);
}

/**
 * Replaces the profile cache with the larger one the plan has room for.
 */
static StatusCode heldEnlargeProfileCache(
	DataSetIpi* dataSet,
	FILE* file,
	const heldPlan* plan) {
	fiftyoneDegreesCollectionConfig config = dataSet->config.profiles;
	if (plan->profileCacheCapacity <= config.capacity) {
		return SUCCESS;
	}
	config.capacity = plan->profileCacheCapacity;
	FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->profiles);
	dataSet->profiles = NULL;
	return createProfilesFromFile(dataSet, file, &config);
}

/**
 * Holds what the plan says to hold. Anything allocated is freed with the data
 * set if this fails.
 */
static StatusCode heldPlanApply(
	DataSetIpi* dataSet,
	FILE* file,
	const heldPlan* plan) {
	StatusCode status = heldCreate(
		&dataSet->strings,
		file,
		(uint64_t)dataSet->header.strings.startPosition,
		1,
		plan->strings,
		plan->stringsCount);
	if (status != SUCCESS) {
		return status;
	}
	status = heldCreate(
		&dataSet->values,
		file,
		(uint64_t)dataSet->header.values.startPosition,
		dataSet->values->elementSize,
		plan->values,
		plan->valuesCount);
	if (status != SUCCESS) {
		return status;
	}
	if (plan->holdProfileGroups) {
		// With no profile groups there is nothing to hold or to read.
		static const fiftyoneDegreesCollectionConfig inMemory = { true, 0, 0 };
		static const fiftyoneDegreesCollectionConfig onFile = { false, 0, 0 };
		FIFTYONE_DEGREES_COLLECTION_FREE(dataSet->profileGroups);
		dataSet->profileGroups = CollectionCreateFromFile(
			file,
			&dataSet->b.b.filePool,
			dataSet->header.profileGroups.length > 0 ? &inMemory : &onFile,
			dataSet->header.profileGroups,
			CollectionReadFileFixed);
		if (dataSet->profileGroups == NULL) {
			return getCollectionFileStatus(
				dataSet,
				dataSet->header.profileGroups,
				0);
		}
	}
	status = heldEnlargeProfileCache(dataSet, file, plan);
	if (status != SUCCESS) {
		return status;
	}
	return heldCreate(
		&dataSet->profiles,
		file,
		(uint64_t)dataSet->header.profiles.startPosition,
		(uint32_t)1 << getProfilesFileShift(dataSet),
		plan->profiles,
		plan->profilesCount);
}

/**
 * Creates the graphs and holds in memory what the required properties read, as
 * far as what is left on file pays for it.
 */
static StatusCode heldInit(DataSetIpi* dataSet, Exception* exception) {
	FILE* file;
	heldPlan plan;
	memset(&plan, 0, sizeof(plan));
	StatusCode status = FileOpen(dataSet->b.b.fileName, &file);
	if (status != SUCCESS) {
		return status;
	}
	status = heldPlanCreate(dataSet, file, &plan, exception);
	if (status == SUCCESS && EXCEPTION_OKAY) {
		// The memory used to work out the plan goes first so that it does not
		// add to the most memory used.
		heldFreeIfSet(plan.buffer);
		plan.buffer = NULL;
		heldFreeIfSet(plan.buckets);
		plan.buckets = NULL;
		status = heldPlanApply(dataSet, file, &plan);
	}
	heldPlanFree(&plan);
	fclose(file);
	return status;
}

/**
 * Gets the number of names in a list of required properties.
 */
static uint32_t heldCountNames(const char* list) {
	uint32_t count = 0;
	bool inName = false;
	for (; *list != '\0'; list++) {
		const bool isName =
			strchr(FIFTYONE_DEGREES_PROPERTIES_SEPARATORS, *list) == NULL;
		if (isName && inName == false) {
			count++;
		}
		inName = isName;
	}
	return count;
}

/**
 * True for the shape of the Balanced preset when the caller has named fewer
 * properties than the data file has. The other presets, and a caller who names
 * nothing, use the configuration as it is.
 */
static bool heldIsWanted(
	const DataSetIpi* dataSet,
	const PropertiesRequired* properties) {
	const ConfigIpi* const config = &dataSet->config;
	if (config->b.allInMemory ||
		config->strings.loaded == false ||
		config->graph.loaded == false ||
		config->values.loaded ||
		config->profiles.loaded ||
		properties == NULL) {
		return false;
	}
	const uint32_t count = dataSet->header.properties.count;
	if (properties->existing != NULL) {
		return properties->existing->count < count;
	}
	if (properties->array != NULL && properties->count > 0) {
		return (uint32_t)properties->count < count;
	}
	return properties->string != NULL &&
		strlen(properties->string) > 0 &&
		heldCountNames(properties->string) < count;
}

static StatusCode readDataSetFromFile(
	DataSetIpi* dataSet,
	FILE* file,
	const PropertiesRequired* properties,
	Exception* exception) {
	StatusCode status = SUCCESS;

	// Copy the bytes that form the header from the start of the memory
	// location to the data set data.ptr provided
	status = readHeaderFromFile(file, &dataSet->header);
	if (status != SUCCESS) {
		return status;
	}

	// Check the version.
	status = checkVersion(dataSet);
	if (status != SUCCESS) {
		return status;
	}

	// Create the strings collection. When only some of the strings will be
	// held, the required properties have to be known first, so the strings read
	// from the file until then.
	const bool hold = heldIsWanted(dataSet, properties);
	const uint32_t stringsCount = dataSet->header.strings.count;
	*(uint32_t*)(&dataSet->header.strings.count) = 0;
	if (hold) {
		static const fiftyoneDegreesCollectionConfig readFromFile = { false, 0, 0 };
		COLLECTION_CREATE_FILE_WITH_CONFIG(
			strings,
			fiftyoneDegreesStoredBinaryValueRead,
			&readFromFile);
		if (getCollectionFileStatus(dataSet, dataSet->header.strings, 0) ==
			CORRUPT_DATA) {
			return CORRUPT_DATA;
		}
	}
	else {
		COLLECTION_CREATE_FILE(strings, fiftyoneDegreesStoredBinaryValueRead);
	}
	*(uint32_t*)(&dataSet->header.strings.count) = stringsCount;

	// Override the header count so that the variable collection can work.
	const uint32_t componentCount = dataSet->header.components.count;
	*(uint32_t*)(&dataSet->header.components.count) = 0;
	COLLECTION_CREATE_FILE(components, fiftyoneDegreesComponentReadFromFile);
	*(uint32_t*)(&dataSet->header.components.count) = componentCount;

	COLLECTION_CREATE_FILE(maps, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE(properties, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE(values, CollectionReadFileFixed);

	status = createProfilesFromFile(dataSet, file, &dataSet->config.profiles);
	if (status != SUCCESS) {
		return status;
	}

	COLLECTION_CREATE_FILE(graphs, CollectionReadFileFixed);

	COLLECTION_CREATE_FILE(profileGroups, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE(propertyTypes, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE(profileOffsets, CollectionReadFileFixed);

	// Which graphs to hold is only known once the required properties are.
	if (hold == false) {
		dataSet->graphsArray = fiftyoneDegreesIpiGraphCreateFromFile(
			dataSet->graphs,
			file,
			&dataSet->b.b.filePool,
			// This is not the configuration for the collection of all graphs,
			// but the configuration for each individual graph.
			dataSet->config.graph,
			exception);
	}

	return initDataSetPost(dataSet, exception);
}

#endif

/**
 * Calculates the highest concurrency value to ensure sufficient file reader
 * handles are generated at initialisation to service the maximum number of
 * concurrent operations.
 * @param config being used for initialisation.
 * @return the highest concurrency value from the configuration, or 1 if no
 * concurrency values are available.
 */
static uint16_t getMaxConcurrency(const ConfigIpi* config) {
	uint16_t concurrency = 1;
	MAX_CONCURRENCY(strings);
	MAX_CONCURRENCY(components);
	MAX_CONCURRENCY(maps);
	MAX_CONCURRENCY(properties);
	MAX_CONCURRENCY(values);
	MAX_CONCURRENCY(profiles);
	MAX_CONCURRENCY(graphs);
	MAX_CONCURRENCY(profileOffsets);
	MAX_CONCURRENCY(propertyTypes);
	MAX_CONCURRENCY(profileGroups);
	MAX_CONCURRENCY(graph);
	return concurrency;
}

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY

static StatusCode initWithFile(
	DataSetIpi* dataSet,
	const PropertiesRequired* properties,
	Exception* exception) {
	StatusCode status;
	FileHandle handle;

	// Initialise the file read for the dataset. Create as many readers as
	// there will be concurrent operations.
	status = FilePoolInit(
		&dataSet->b.b.filePool,
		dataSet->b.b.fileName,
		getMaxConcurrency(&dataSet->config),
		exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}

	// Create a new file handle for the read operation. The file handle can't
	// come from the pool of handles because there may only be one available
	// in the pool and it will be needed for some initialisation activities.
	status = FileOpen(dataSet->b.b.fileName, &handle.file);
	if (status != SUCCESS) {
		return status;
	}

	// Read the data set from the source.
	status = readDataSetFromFile(dataSet, handle.file, properties, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		fclose(handle.file);
		return status;
	}

	// Before closing the file handle, clean up any other temp files which are
	// not in use.
#ifndef __APPLE__
	if (dataSet->config.b.useTempFile == true) {
		FileDeleteUnusedTempFiles(
			dataSet->b.b.masterFileName,
			dataSet->config.b.tempDirs,
			dataSet->config.b.tempDirCount,
			sizeof(DataSetIpiHeader));
	}
#endif
	// Close the file handle.
	fclose(handle.file);

	return status;
}

#endif

static StatusCode initDataSetFromFile(
	void* dataSetBase,
	const void* configBase,
	PropertiesRequired* properties,
	const char* fileName,
	Exception* exception) {
	DataSetIpi* dataSet = (DataSetIpi*)dataSetBase;
	ConfigIpi* config = (ConfigIpi*)configBase;
	StatusCode status = NOT_SET;

	// Common data set initialisation actions.
	initDataSet(dataSet, &config);

	// Initialise the super data set with the filename and configuration
	// provided.
	status = DataSetInitFromFile(
		&dataSet->b.b,
		fileName,
		sizeof(DataSetIpiHeader));
	if (status != SUCCESS) {
		return status;
	}

	// If there is no collection configuration the the entire data file should
	// be loaded into memory. Otherwise use the collection configuration to
	// partially load data into memory and cache the rest.
	if (config->b.allInMemory == true) {
		status = initInMemory(dataSet, exception);
	}
	else {
#ifndef FIFTYONE_DEGREES_MEMORY_ONLY
		status = initWithFile(dataSet, properties, exception);
#else
		status = INVALID_CONFIG;
#endif
	}

	// Return the status code if something has gone wrong.
	if (status != SUCCESS || EXCEPTION_FAILED) {
		// Delete the temp file if one has been created.
		if (config->b.useTempFile == true) {
			FileDelete(dataSet->b.b.fileName);
		}
		return status;
	}

	// Initialise the required properties and headers and check the 
	// initialisation was successful.
	status = initPropertiesAndHeaders(dataSet, properties, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		// Delete the temp file if one has been created.
		if (config->b.useTempFile == true) {
			FileDelete(dataSet->b.b.fileName);
		}
		return status;
	}

	// Initialise the components available to flag which components have 
	// properties which are to be returned (i.e. available properties).
	status = initComponentsAvailable(dataSet, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		if (config->b.useTempFile == true) {
			FileDelete(dataSet->b.b.fileName);
		}
		return status;
	}

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY
	// Now the required properties are known, hold what they read.
	if (heldIsWanted(dataSet, properties)) {
		status = heldInit(dataSet, exception);
		if (status != SUCCESS || EXCEPTION_FAILED) {
			if (config->b.useTempFile == true) {
				FileDelete(dataSet->b.b.fileName);
			}
			return status;
		}
	}
#endif

	// Check there are properties available for retrieval.
	if (dataSet->b.b.available->count == 0) {
		// Delete the temp file if one has been created.
		if (config->b.useTempFile == true) {
			FileDelete(dataSet->b.b.fileName);
		}
		return status;
	}
	return status;
}

fiftyoneDegreesStatusCode fiftyoneDegreesIpiInitManagerFromFile(
	fiftyoneDegreesResourceManager* manager,
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	const char* fileName,
	fiftyoneDegreesException* exception) {

	// The init methods show some failures only in the exception, so they are
	// always given one, and the status returned carries those failures.
	Exception ownException;
	if (exception == NULL) {
		exception = &ownException;
		EXCEPTION_CLEAR;
	}

	DataSetIpi* dataSet = (DataSetIpi*)Malloc(sizeof(DataSetIpi));
	if (dataSet == NULL) {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
		return INSUFFICIENT_MEMORY;
	}

	StatusCode status = initDataSetFromFile(
		dataSet,
		config,
		properties,
		fileName,
		exception);
	if (status == SUCCESS) {
		status = getFailureStatus(status, exception);
	}
	if (status != SUCCESS) {
		// Nothing below has taken ownership of the data set, so it is freed
		// here on every failure. Without this the file path leaks it along
		// with the file pool's open handles, which is the path every data
		// file of an earlier version now takes.
		freeDataSet(dataSet);
		return status;
	}
	ResourceManagerInit(manager, dataSet, &dataSet->b.b.handle, freeDataSet);
	if (dataSet->b.b.handle == NULL) {
		freeDataSet(dataSet);
		status = INSUFFICIENT_MEMORY;
	}
	return status;
}

size_t fiftyoneDegreesIpiSizeManagerFromFile(
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	const char* fileName,
	fiftyoneDegreesException* exception) {
	size_t allocated;
	ResourceManager manager;
	StatusCode status;

	// Set the memory allocation and free methods for tracking.
	MemoryTrackingReset();
	Malloc = MemoryTrackingMalloc;
	MallocAligned = MemoryTrackingMallocAligned;
	Free = MemoryTrackingFree;
	FreeAligned = MemoryTrackingFreeAligned;

	// Initialise the manager with the tracking methods in use to determine
	// the amount of memory that is allocated. The init method has freed its
	// memory and not set up the manager unless it worked, so only then is
	// there a manager to free. Otherwise the caller needs the status.
	status = IpiInitManagerFromFile(
		&manager,
		config,
		properties,
		fileName,
		exception);
	if (status != SUCCESS) {
		EXCEPTION_SET(status);
	}
	else if (EXCEPTION_OKAY) {
		ResourceManagerFree(&manager);
	}

	// Get the total maximum amount of allocated memory needed for the manager
	// and associated resources.
	allocated = MemoryTrackingGetMax();

	// Check that all the memory has been freed.
	assert(MemoryTrackingGetAllocated() == 0);

	// Return the malloc and free methods to standard operation.
	Malloc = MemoryStandardMalloc;
	MallocAligned = MemoryStandardMallocAligned;
	Free = MemoryStandardFree;
	FreeAligned = MemoryStandardFreeAligned;
	MemoryTrackingReset();

	return allocated;
}

static StatusCode initDataSetFromMemory(
	void* dataSetBase,
	const void* configBase,
	PropertiesRequired* properties,
	void* memory,
	FileOffset size,
	Exception* exception) {
	StatusCode status = SUCCESS;
	MemoryReader reader;
	DataSetIpi* dataSet = (DataSetIpi*)dataSetBase;
	ConfigIpi* config = (ConfigIpi*)configBase;

	// Common data set initialisation actions.
	initDataSet(dataSet, &config);

	// If memory is to be freed when the data set is freed then record the 
	// pointer to the memory location for future reference.
	if (dataSet->config.b.freeData == true) {
		dataSet->b.b.memoryToFree = memory;
	}

	// Set up the reader.
	reader.startByte = reader.current = (byte*)memory;
	reader.length = size;
	reader.lastByte = reader.current + size;

	// Initialise the data set from the memory reader.
	status = initWithMemory(dataSet, &reader, exception);

	// Return the status code if something has gone wrong.
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}

	// Initialise the required properties and headers.
	status = initPropertiesAndHeaders(dataSet, properties, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}

	// Initialise the components available to flag which components have
	// properties which are to be returned (i.e. available properties).
	status = initComponentsAvailable(dataSet, exception);

	return status;
}

fiftyoneDegreesStatusCode fiftyoneDegreesIpiInitManagerFromMemory(
	fiftyoneDegreesResourceManager* manager,
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	void* memory,
	FileOffset size,
	fiftyoneDegreesException* exception) {
	// The init methods show some failures only in the exception, so they are
	// always given one, and the status returned carries those failures.
	Exception ownException;
	if (exception == NULL) {
		exception = &ownException;
		EXCEPTION_CLEAR;
	}

	DataSetIpi* dataSet = (DataSetIpi*)Malloc(sizeof(DataSetIpi));
	if (dataSet == NULL) {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
		return INSUFFICIENT_MEMORY;
	}

	StatusCode status = initDataSetFromMemory(
		dataSet,
		config,
		properties,
		memory,
		size,
		exception);
	if (status == SUCCESS) {
		status = getFailureStatus(status, exception);
	}
	if (status != SUCCESS) {
		// The memory stays with the caller until the data set is set up, so
		// it is not freed with what the data set allocated.
		dataSet->b.b.memoryToFree = NULL;
		freeDataSet(dataSet);
		return status;
	}
	ResourceManagerInit(manager, dataSet, &dataSet->b.b.handle, freeDataSet);
	if (dataSet->b.b.handle == NULL)
	{
		dataSet->b.b.memoryToFree = NULL;
		freeDataSet(dataSet);
		status = INSUFFICIENT_MEMORY;
	}
	return status;
}

size_t fiftyoneDegreesIpiSizeManagerFromMemory(
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	void* memory,
	FileOffset size,
	fiftyoneDegreesException* exception) {
	size_t allocated;
	ResourceManager manager;
	StatusCode status;

	// Set the memory allocation and free methods for tracking.
	MemoryTrackingReset();
	Malloc = MemoryTrackingMalloc;
	MallocAligned = MemoryTrackingMallocAligned;
	Free = MemoryTrackingFree;
	FreeAligned = MemoryTrackingFreeAligned;

	// Ensure that the memory used is not freed with the data set. A null
	// configuration means the balanced one, as it does in initDataSet.
	ConfigIpi sizeConfig = config != NULL ? *config : IpiBalancedConfig;
	sizeConfig.b.freeData = false;

	// Initialise the manager with the tracking methods in use to determine
	// the amount of memory that is allocated. The init method has freed its
	// memory and not set up the manager unless it worked, so only then is
	// there a manager to free. Otherwise the caller needs the status.
	status = IpiInitManagerFromMemory(
		&manager,
		&sizeConfig,
		properties,
		memory,
		size,
		exception);
	if (status != SUCCESS) {
		EXCEPTION_SET(status);
	}
	else if (EXCEPTION_OKAY) {
		ResourceManagerFree(&manager);
	}

	// Get the total maximum amount of allocated memory needed for the manager
	// and associated resources.
	allocated = MemoryTrackingGetMax();

	// Check that all the memory has been freed.
	assert(MemoryTrackingGetAllocated() == 0);

	// Return the malloc and free methods to standard operation.
	Malloc = MemoryStandardMalloc;
	MallocAligned = MemoryStandardMallocAligned;
	Free = MemoryStandardFree;
	FreeAligned = MemoryStandardFreeAligned;
	MemoryTrackingReset();

	return allocated;
}

fiftyoneDegreesDataSetIpi* fiftyoneDegreesDataSetIpiGet(fiftyoneDegreesResourceManager* manager) {
	return (DataSetIpi*)DataSetGet(manager);
}

void fiftyoneDegreesDataSetIpiRelease(fiftyoneDegreesDataSetIpi* dataSet) {
	DataSetRelease(&dataSet->b.b);
}

/**
 * Definition of the reload methods from the data set macro.
 */
FIFTYONE_DEGREES_DATASET_RELOAD(Ipi)


/**
 * IP INTELLIGENCE RESULTS METHODS
 */


/* 
 * The WeightedItemList functions are in src/common/weightedItem.h:
 * - WeightedItemListInit() 
 * - WeightedItemListRelease()
 * - WeightedItemListFree()
 * - WeightedItemListExtend()
 * - WeightedItemListAdd()
 */

/**
 * Results methods
 */

/**
 * Marks all the entries in the has values cache as unknown. Must be called
 * whenever the results are repopulated as the cached outcomes only relate
 * to the results they were recorded for.
 */
static void resultsIpiResetHasValuesCache(ResultsIpi* const results) {
	if (results->hasValuesCache != NULL) {
		memset(
			results->hasValuesCache,
			IPI_HAS_VALUES_UNKNOWN,
			results->hasValuesCacheCount);
	}
}

fiftyoneDegreesResultsIpi* fiftyoneDegreesResultsIpiCreate(
	fiftyoneDegreesResourceManager* manager) {
	ResultsIpi* results;
	DataSetIpi* dataSet;

	// Increment the inUse counter for the active data set so that we can
	// track any results that are created.
	dataSet = (DataSetIpi*)DataSetGet(manager);

	// Create a new instance of results.
	FIFTYONE_DEGREES_ARRAY_CREATE(
		ResultIpi,
		results,
		dataSet->componentsAvailableCount);

	if (results != NULL) {

		// Initialise the results.
		fiftyoneDegreesResultsInit(&results->b, (void*)(&dataSet->b));

		// Reset the property and values list ready for first use sized for
		// a single value to be returned.
		WeightedItemListInit(&results->values, 1, FIFTYONE_DEGREES_WEIGHTED_ITEM_LIST_DEFAULT_LOAD_FACTOR);
		DataReset(&results->propertyItem.data);

		// Allocate the cache used to avoid repeating the profile walks
		// which check whether a property has values for the current
		// results. If the allocation fails the cache is not used.
		results->hasValuesCache = NULL;
		results->hasValuesCacheCount = 0;
		if (dataSet->b.b.available != NULL &&
			dataSet->b.b.available->count > 0) {
			results->hasValuesCache = (byte*)Malloc(
				dataSet->b.b.available->count);
			if (results->hasValuesCache != NULL) {
				results->hasValuesCacheCount =
					dataSet->b.b.available->count;
				resultsIpiResetHasValuesCache(results);
			}
		}

		// Reading a profile through a cache takes a lock twice, so one read
		// for each result is kept for all the properties of that result. It is
		// a copy because a cache entry held between calls could leave a shard
		// of the cache with no entry to give. If this allocation fails each
		// property reads the cache.
		results->keptProfiles = NULL;
		if (dataSet->config.profiles.loaded == false &&
			dataSet->config.profiles.capacity > 0 &&
			results->capacity > 0) {
			results->keptProfiles = (byte*)Malloc(
				(size_t)results->capacity * IPI_KEPT_PROFILE_BYTES);
		}
	}
	else {
		DataSetRelease((DataSetBase *)dataSet);
	}

	return results;
}

static void resultsIpiRelease(ResultsIpi* results) {
	if (results->propertyItem.data.ptr != NULL &&
		results->propertyItem.collection != NULL) {
		COLLECTION_RELEASE(
			results->propertyItem.collection,
			&results->propertyItem);
	}
	WeightedItemListRelease(&results->values);
}

void fiftyoneDegreesResultsIpiFree(fiftyoneDegreesResultsIpi* results) {
	resultsIpiRelease(results);
	WeightedItemListFree(&results->values);
	if (results->hasValuesCache != NULL) {
		Free(results->hasValuesCache);
		results->hasValuesCache = NULL;
		results->hasValuesCacheCount = 0;
	}
	if (results->keptProfiles != NULL) {
		Free(results->keptProfiles);
		results->keptProfiles = NULL;
	}
	DataSetRelease((DataSetBase*)results->b.dataSet);
	Free(results);
}

/**
 * True if the IPv6 address is in ::ffff:0:0/96, which represents the IPv4
 * address in its last four bytes. See
 * https://github.com/51Degrees/specifications/blob/main/ip-intelligence-specification/pipeline-elements/ip-intelligence-on-premise.md#address-forms
 */
static bool isIpv4Mapped(const unsigned char* const ipAddress) {
	static const byte prefix[IPV6_LENGTH - IPV4_LENGTH] =
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
	return memcmp(ipAddress, prefix, sizeof(prefix)) == 0;
}

static bool addResultsFromIpAddressNoChecks(
	ResultsIpi* results,
	const unsigned char* ipAddress,
	fiftyoneDegreesIpType type,
	uint32_t componentMask,
	fiftyoneDegreesException* exception) {
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	// Every lookup comes through here, so an IPv4-mapped address takes the
	// answers of its IPv4 address however it was given.
	if (type == IP_TYPE_IPV6 && isIpv4Mapped(ipAddress)) {
		ipAddress += IPV6_LENGTH - IPV4_LENGTH;
		type = IP_TYPE_IPV4;
	}
	for (uint32_t componentIndex = 0;
		componentIndex < dataSet->componentsList.count;
		componentIndex++) {
		if (!dataSet->componentsAvailable[componentIndex]) {
			continue;
		}
		const Component * const component = COMPONENT(dataSet, componentIndex);
		if (!component) {
			continue;
		}
		ResultIpi * const nextResult = &(results->items[results->count]);
		results->count++;
		resultIpiReset(nextResult);
		// Default IP range offset
		nextResult->graphResult = FIFTYONE_DEGREES_IPI_CG_RESULT_DEFAULT;
		nextResult->graphResult.rawOffset = NULL_PROFILE_OFFSET; // Default IP range offset
		nextResult->targetIpAddress.type = type;
		nextResult->type = type;
		// Record the component the result relates to so that property
		// lookups can be routed to the result for the component that owns
		// the property.
		nextResult->componentIndex = (int32_t)componentIndex;

		if (type == IP_TYPE_IPV4) {
			// We only get the exact length of ipv4
			memset(nextResult->targetIpAddress.value, 0, IPV6_LENGTH);
			memcpy(nextResult->targetIpAddress.value, ipAddress, IPV4_LENGTH);
		}
		else {
			// We only get the exact length of ipv6
			memcpy(nextResult->targetIpAddress.value, ipAddress, IPV6_LENGTH);
		}

		// Only evaluate the graph for a component the caller will read. The
		// result slot is kept so the positional mapping to components is
		// unchanged, and the slot reads as a null profile.
		if (COMPONENT_MASK_ENABLED(componentMask, componentIndex)) {
			setResultFromIpAddress(
				nextResult,
				dataSet,
				component->componentId,
				exception);
			if (EXCEPTION_FAILED) {
				return false;
			}
		}
	}
	return true;
}

void fiftyoneDegreesResultsIpiFromIpAddressForProperties(
	fiftyoneDegreesResultsIpi* results,
	const unsigned char* ipAddress,
	size_t ipAddressLength,
	fiftyoneDegreesIpType type,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount,
	fiftyoneDegreesException* exception) {

	// Make sure the input is always in the correct format
	if (type == IP_TYPE_INVALID
		|| (type == IP_TYPE_IPV4
			&& ipAddressLength < IPV4_LENGTH)
		|| (type == IP_TYPE_IPV6
			&& ipAddressLength < IPV6_LENGTH)) {
		EXCEPTION_SET(INCORRECT_IP_ADDRESS_FORMAT);
		return;
	}

	// Reset the results data before iterating the evidence.
	results->count = 0;
	resultsIpiResetHasValuesCache(results);

	addResultsFromIpAddressNoChecks(
		results,
		ipAddress,
		type,
		PropertiesGetComponentMask(
			((DataSetIpi*)results->b.dataSet)->b.b.available,
			requiredPropertyIndexes,
			requiredPropertyIndexesCount),
		exception);
}

void fiftyoneDegreesResultsIpiFromIpAddress(
	fiftyoneDegreesResultsIpi* results,
	const unsigned char* ipAddress,
	size_t ipAddressLength,
	fiftyoneDegreesIpType type,
	fiftyoneDegreesException* exception) {
	fiftyoneDegreesResultsIpiFromIpAddressForProperties(
		results,
		ipAddress,
		ipAddressLength,
		type,
		NULL,
		-1,
		exception);
}

void fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
	fiftyoneDegreesResultsIpi* results,
	const char* ipAddress,
	size_t ipLength,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount,
	fiftyoneDegreesException* exception) {
	IpAddress ip;
	const bool parsed =
		IpAddressParse(ipAddress, ipAddress + ipLength, &ip);
	// Check if the IP address was successfully created
	if (!parsed) {
		EXCEPTION_SET(INCORRECT_IP_ADDRESS_FORMAT);
		return;
	}
	
	// Perform the search on the IP address byte array
	switch(ip.type) {
	case IP_TYPE_IPV4:
		fiftyoneDegreesResultsIpiFromIpAddressForProperties(
			results,
			ip.value,
			IPV4_LENGTH,
			IP_TYPE_IPV4,
			requiredPropertyIndexes,
			requiredPropertyIndexesCount,
			exception);
		break;
	case IP_TYPE_IPV6:
		fiftyoneDegreesResultsIpiFromIpAddressForProperties(
			results,
			ip.value,
			IPV6_LENGTH,
			IP_TYPE_IPV6,
			requiredPropertyIndexes,
			requiredPropertyIndexesCount,
			exception);
		break;
	case IP_TYPE_INVALID:
	default:
		EXCEPTION_SET(INCORRECT_IP_ADDRESS_FORMAT);
		break;
	}
}

void fiftyoneDegreesResultsIpiFromIpAddressString(
	fiftyoneDegreesResultsIpi* results,
	const char* ipAddress,
	size_t ipLength,
	fiftyoneDegreesException* exception) {
	fiftyoneDegreesResultsIpiFromIpAddressStringForProperties(
		results,
		ipAddress,
		ipLength,
		NULL,
		-1,
		exception);
}

static bool setResultsFromEvidence(
	void* state,
	EvidenceKeyValuePair* pair) {
	const stateWithUniqueHeaderIndex* indexState = (stateWithUniqueHeaderIndex*)state;
	const stateWithException* exceptionState = (stateWithException*)indexState->subState;
	const resultsWithComponentMask* masked =
		(resultsWithComponentMask*)exceptionState->state;
	ResultsIpi* results = masked->results;
	Exception* exception = exceptionState->exception;
	// We should not look further if a 
	// result has already been found
	if (results->count == 0) {
		const DataSetIpi* dataSet = (DataSetIpi*)results->b.dataSet;
		const uint32_t curHeaderIndex = indexState->headerIndex;
		const int headerIndex = HeaderGetIndex(
			dataSet->b.b.uniqueHeaders,
			pair->item.key,
			pair->item.keyLength);
		// Only the current header index should be considered
		if (headerIndex >= 0 && headerIndex == (int)curHeaderIndex) {
			// Get the parsed Value
			const char *ipAddressString = (const char *)pair->parsedValue;
			// Obtain the byte array first
			IpAddress ipAddress;
			const bool parsed =
				fiftyoneDegreesIpAddressParse(ipAddressString, ipAddressString + strlen(ipAddressString), &ipAddress);
			// Check if the IP address was successfully created
			if (!parsed || (ipAddress.type == IP_TYPE_INVALID)) {
				EXCEPTION_SET(INCORRECT_IP_ADDRESS_FORMAT);
				return false;
			}

			// Configure the next result in the array of results.
			addResultsFromIpAddressNoChecks(
				results,
				ipAddress.value,
				ipAddress.type,
				masked->componentMask,
				exception);
		}
	}

	return EXCEPTION_OKAY;
}

static void fiftyoneDegreesIterateHeadersWithEvidence(
	ResultsIpi* const results,
	EvidenceKeyValuePairArray* evidence,
	int prefixes,
	stateWithUniqueHeaderIndex *state) {

	const DataSetIpi * const dataSet = (DataSetIpi *)results->b.dataSet;
	const uint32_t headersCount = dataSet->b.b.uniqueHeaders ? dataSet->b.b.uniqueHeaders->count : 0;

	// Each unique header is checked against the evidence
	// in the order that its added to the headers array.
	// The order represents the prioritis of the headers.
	for (uint32_t i = 0;
		i < headersCount && results->count == 0;
		i++) {
		state->headerIndex = i;
		EvidenceIterate(
			evidence,
			prefixes,
			state,
			setResultsFromEvidence);
	}
}

void fiftyoneDegreesResultsIpiFromEvidenceForProperties(
	fiftyoneDegreesResultsIpi* results,
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount,
	fiftyoneDegreesException* exception) {
	stateWithException subState;
	stateWithUniqueHeaderIndex state;
	resultsWithComponentMask masked;
	masked.results = results;
	masked.componentMask = PropertiesGetComponentMask(
		((DataSetIpi*)results->b.dataSet)->b.b.available,
		requiredPropertyIndexes,
		requiredPropertyIndexesCount);
	subState.state = &masked;
	subState.exception = exception;
	state.subState = &subState;

	if (evidence != (EvidenceKeyValuePairArray*)NULL) {
		// Reset the results data before iterating the evidence.
		results->count = 0;
		resultsIpiResetHasValuesCache(results);

		fiftyoneDegreesIterateHeadersWithEvidence(
			results,
			evidence,
			FIFTYONE_DEGREES_EVIDENCE_QUERY,
			&state);
		if (EXCEPTION_FAILED) {
			return;
		}

		// If no results were obtained from the query evidence prefix then use
		// the HTTP headers to populate the results.
		if (results->count == 0) {
			fiftyoneDegreesIterateHeadersWithEvidence(
				results,
				evidence,
				FIFTYONE_DEGREES_EVIDENCE_SERVER,
				&state);
			if (EXCEPTION_FAILED) {
				return;
			}
		}
	}
}

void fiftyoneDegreesResultsIpiFromEvidence(
	fiftyoneDegreesResultsIpi* results,
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence,
	fiftyoneDegreesException* exception) {
	fiftyoneDegreesResultsIpiFromEvidenceForProperties(
		results,
		evidence,
		NULL,
		-1,
		exception);
}

static bool addWeightedValue(
	ResultsIpi* results,
	Item* item,
	uint16_t rawWeighting,
	Exception* exception) {
	Item valueItem;
	WeightedItem weightedItem;
	const DataSetIpi* dataSet = (DataSetIpi*)results->b.dataSet;
	const Value* value = (Value*)item->data.ptr;
	if (value != NULL) {
		if (results->values.count == results->values.capacity) {
			WeightedItemListExtend(
				&results->values,
				results->values.capacity
				* FIFTYONE_DEGREES_WEIGHTED_ITEM_LIST_RESIZE_FACTOR,
				exception);
			if (EXCEPTION_FAILED) {
				COLLECTION_RELEASE(dataSet->values, item);
				return false;
			}
		}
		PropertyValueType const storedValueType = PropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			value->propertyIndex,
			exception);
		if (EXCEPTION_OKAY) {
			DataReset(&valueItem.data);
			const uint16_t valueRawWeight = ValueGetWeight(value);
			const uint16_t valueWeight = valueRawWeight ? valueRawWeight : 0xFFFFU;
			if (StoredBinaryValueGet(
				dataSet->strings,
				value->nameOffset,
				storedValueType,
				&valueItem,
				exception) != NULL && EXCEPTION_OKAY) {
				weightedItem.item = valueItem;
				weightedItem.rawWeighting = ((uint32_t)rawWeighting) * (uint32_t)valueWeight;
				WeightedItemListAdd(&results->values, &weightedItem, exception);
			}
		}
	}
	COLLECTION_RELEASE(dataSet->values, item);
	return EXCEPTION_OKAY;
}

static bool addWeightedValueWithState(void* state, Item* item) {
	// The results values are a list of collection items and their weighting.
	// The weighting cannot be passed along with Item as this is the profile
	// standard in the common library. Thus the weighting is passed along with
	// the state.
	const stateWithWeighting* weightingState = (stateWithWeighting*)((stateWithException*)state)->state;
	ResultsIpi* results =
		(ResultsIpi*)weightingState->subState;
	Exception* const exception = ((stateWithException*)state)->exception;

	return addWeightedValue(
		results,
		item,
		weightingState->rawWeighting,
		exception);
}

static uint32_t addValuesFromProfile(
	DataSetIpi* dataSet,
	ResultsIpi* results,
	Profile* profile,
	Property* property,
	uint16_t rawWeighting,
	Exception* exception) {
	uint32_t count;

	// Set the state for the callbacks.
	stateWithException state;
	stateWithWeighting weightingState;
	weightingState.subState = results;
	weightingState.rawWeighting = rawWeighting;
	state.state = &weightingState;
	state.exception = exception;

	// Iterate over the values associated with the property adding them
	// to the list of values. Get the number of values available as 
	// this will be used to increase the size of the list should there
	// be insufficient space.
	count = ProfileIterateValuesForProperty(
		dataSet->values,
		profile,
		property,
		&state,
		addWeightedValueWithState,
		exception);
	EXCEPTION_THROW;

	// The count of values should always be lower or the same as the profile
	// count. i.e. there can never be more values counted than there are values
	// against the profile.
	assert(count <= profile->valueCount);

	return count;
}

static uint32_t addValuesFromSingleProfile(
	ResultsIpi* results,
	Property *property,
	uint32_t profileOffset,
	uint16_t rawWeighting,
	Exception* exception) {
	uint32_t count = 0;
	Item profileItem;
	DataSetIpi* dataSet = (DataSetIpi*)results->b.dataSet;

	// Add values from profiles
	Profile *profile = NULL;
	if (profileOffset != NULL_PROFILE_OFFSET) {
		DataReset(&profileItem.data);
		const CollectionKey profileKey = {
			profileOffset,
			CollectionKeyType_Profile,
		};
		profile = (Profile*)dataSet->profiles->get(
			dataSet->profiles,
			&profileKey,
			&profileItem,
			exception);
		// If profile is found
		if (profile != NULL && EXCEPTION_OKAY) {
			count += addValuesFromProfile(
				dataSet,
				results,
				profile,
				property,
				rawWeighting,
				exception);
			COLLECTION_RELEASE(dataSet->profiles, &profileItem);
		}
	}
	return count;
}

static const CollectionKeyType CollectionKeyType_OffsetPercentage = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_OFFSET_PERCENTAGE,
	sizeof(offsetPercentage),
	NULL,
};

static uint32_t addValuesFromProfileGroup(
	ResultsIpi * const results,
	Property * const property,
	const uint32_t profileGroupOffset,
	Exception * const exception) {
	uint32_t count = 0;
	const DataSetIpi * const dataSet = (const DataSetIpi*)results->b.dataSet;

	if (profileGroupOffset == NULL_PROFILE_OFFSET) {
		return 0;
	}
	Item profileGroupItem;
	DataReset(&profileGroupItem.data);

	const Collection * const profileGroups = dataSet->profileGroups;
	for (uint32_t totalWeight = 0, nextOffset = profileGroupOffset;
		(totalWeight < FULL_RAW_WEIGHTING) && EXCEPTION_OKAY;
		++nextOffset) {
		const CollectionKey profileGroupKey = {
			nextOffset,
			&CollectionKeyType_OffsetPercentage,
		};
		const offsetPercentage* const nextWeightedProfileOffset = (const offsetPercentage*)profileGroups->get(
			profileGroups,
			&profileGroupKey,
			&profileGroupItem,
			exception);
		if (!(nextWeightedProfileOffset && EXCEPTION_OKAY)) {
			break;
		}
		totalWeight += nextWeightedProfileOffset->rawWeighting;
		if (totalWeight <= FULL_RAW_WEIGHTING) {
			count += addValuesFromSingleProfile(
				results,
				property,
				nextWeightedProfileOffset->offset,
				nextWeightedProfileOffset->rawWeighting,
				exception);
		} else {
			EXCEPTION_SET(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA);
		}
		COLLECTION_RELEASE(dataSet->profileGroups, &profileGroupItem);
	}
	return count;
}

static uint32_t getProfileOffset(
	Collection * const profileOffsets,
	const uint32_t offsetIndex,
	Exception * const exception) {

	Item item;
	DataReset(&item.data);
	const CollectionKey resultKey = {
		offsetIndex,
		CollectionKeyType_Integer,
	};
	const uint32_t * const resultRef = (uint32_t*)profileOffsets->get(
		profileOffsets,
		&resultKey,
		&item,
		exception);
	if (!(resultRef && EXCEPTION_OKAY)) {
		return 0;
	}
	const uint32_t result = *resultRef;
	COLLECTION_RELEASE(profileOffsets, &item);
	return result;
}

/**
 * Gets the profile of a result that is not a profile group, or NULL if it has
 * none. A profile read through a cache is kept until the results are next
 * populated, unless it is too large for its slot. The caller passes what it is
 * given to releaseResultProfile when it has finished with the profile.
 */
static const Profile* getResultProfile(
	ResultsIpi* results,
	ResultIpi* result,
	Item* item,
	Exception* exception) {
	const DataSetIpi* const dataSet = (const DataSetIpi*)results->b.dataSet;
	DataReset(&item->data);
	item->handle = NULL;
	item->collection = NULL;
	if (result->keptProfile != NULL) {
		return (const Profile*)result->keptProfile;
	}
	const uint32_t offset = getProfileOffset(
		dataSet->profileOffsets,
		result->graphResult.offset,
		exception);
	if (EXCEPTION_FAILED || offset == NULL_PROFILE_OFFSET) {
		return NULL;
	}
	const CollectionKey key = { offset, CollectionKeyType_Profile };
	const Profile* const profile = (const Profile*)dataSet->profiles->get(
		dataSet->profiles,
		&key,
		item,
		exception);
	if (profile == NULL || EXCEPTION_FAILED || results->keptProfiles == NULL) {
		return profile;
	}
	if (item->handle == NULL) {
		// Held in memory for as long as the data set is.
		result->keptProfile = (const byte*)profile;
	}
	else {
		const size_t bytes =
			sizeof(Profile) + (size_t)profile->valueCount * sizeof(uint32_t);
		if (bytes > IPI_KEPT_PROFILE_BYTES) {
			return profile;
		}
		byte* const slot = results->keptProfiles +
			(size_t)(result - results->items) * IPI_KEPT_PROFILE_BYTES;
		memcpy(slot, profile, bytes);
		result->keptProfile = slot;
	}
	COLLECTION_RELEASE(dataSet->profiles, item);
	item->collection = NULL;
	return (const Profile*)result->keptProfile;
}

static void releaseResultProfile(const DataSetIpi* dataSet, Item* item) {
	if (item->collection != NULL) {
		COLLECTION_RELEASE(dataSet->profiles, item);
	}
}

/**
 * Achieves the same as addValuesFromResult, but gets the value from the
 * default value in the property. This is used when there is no value in
 * the profile, but the property is mandatory.
 */
static WeightedItem* getDefaultValue(
	ResultsIpi* results,
	Property* property,
	Exception* exception) {
	const DataSetIpi* const dataSet = (DataSetIpi*)results->b.dataSet;
	bool added = false;
	Item valueItem;

	DataReset(&valueItem.data);

	// Get the value from the value index and call the callback. Do not 
	// free the item as the calling function is responsible for this.
	const CollectionKey valueKey = {
		property->defaultValueIndex,
		CollectionKeyType_Value,
	};
	if (dataSet->values->get(
		dataSet->values,
		&valueKey,
		&valueItem,
		exception) != NULL &&
		EXCEPTION_OKAY) {
		added = addWeightedValue(
			results,
			&valueItem,
			FULL_RAW_WEIGHTING,
			exception);
		COLLECTION_RELEASE(dataSet->values, &valueItem);
	}
	return added ? results->values.items : NULL;
}


static uint32_t addValuesFromResult(
	ResultsIpi* results,
	ResultIpi* result,
	Property* property,
	Exception* exception) {
	uint32_t count = 0;
	const DataSetIpi* const dataSet = (DataSetIpi*)results->b.dataSet;

	if (results->count > 0) {
		if (result->graphResult.rawOffset != NULL_PROFILE_OFFSET) {
			if (!result->graphResult.isGroupOffset) {
				Item profileItem;
				const Profile* const profile = getResultProfile(
					results,
					result,
					&profileItem,
					exception);
				if (profile != NULL && EXCEPTION_OKAY) {
					count += addValuesFromProfile(
						(DataSetIpi*)dataSet,
						results,
						(Profile*)profile,
						property,
						FULL_RAW_WEIGHTING,
						exception);
					releaseResultProfile(dataSet, &profileItem);
				}
			} else {
				count += addValuesFromProfileGroup(
					results,
					property,
					result->graphResult.offset,
					exception);
			}
		}
	}
	return count;
}

static WeightedItem* getValuesFromResult(
	ResultsIpi* results,
	ResultIpi* result,
	Property* property,
	Exception* exception) {
	// There is a profile available for the property requested. 
	// Use this to add the values to the results.
	addValuesFromResult(results, result, property, exception);

	// Return the first value in the list of items.
	return results->values.items;
}

const fiftyoneDegreesWeightedItem* fiftyoneDegreesResultsIpiGetValues(
	fiftyoneDegreesResultsIpi* const results,
	int const requiredPropertyIndex,
	fiftyoneDegreesException* const exception) {
	Property* property;
	DataSetIpi* dataSet;
	const WeightedItem* firstValue = NULL;

	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	dataSet = (DataSetIpi*)results->b.dataSet;

	// Work out the property index from the required property index.
	const uint32_t propertyIndex = PropertiesGetPropertyIndexFromRequiredIndex(
		dataSet->b.b.available,
		requiredPropertyIndex);

	if (propertyIndex >= 0) {
		// Set the property that will be available in the results structure. 
		// This may also be needed to work out which of a selection of results 
		// are used to obtain the values.
		property = PropertyGet(
			dataSet->properties,
			propertyIndex,
			&results->propertyItem,
			exception);

		if (property != NULL && EXCEPTION_OKAY) {
			// Ensure there is a collection available to the property item so
			// that it can be freed when the results are freed.
			if (results->propertyItem.collection == NULL) {
				results->propertyItem.collection = dataSet->properties;
			}

			// Only the result for the component that owns the property can
			// hold values for it. Profiles only carry values for properties
			// of their own component, so the results for other components
			// are skipped.
			for (uint32_t i = 0; i < results->count && EXCEPTION_OKAY; i++) {
				if (results->items[i].componentIndex ==
					(int32_t)property->componentIndex) {
					firstValue = getValuesFromResult(
						results,
						&results->items[i],
						property,
						exception);
				}
			}

			if (results->values.count == 0 &&
				property->defaultValueIndex != UINT32_MAX &&
				property->isMandatory) {
				// There are no values, but the default value from the property
				// should be used, as there is a default value, and the property
				// is marked mandatory.
				firstValue = getDefaultValue(
					results,
					property,
					exception);
			}
		}
	}

	if (firstValue == NULL) {
		// There are no values for the property requested. Reset the values 
		// list to zero count.
		WeightedItemListRelease(&results->values);
	}
	return firstValue;
}

static bool visitProfilePropertyValue(
	void *state,
	fiftyoneDegreesCollectionItem *item) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(item);
#	endif

	*((bool *)state) = true; // found
	return false; // break
}

static bool profileHasValidPropertyValue(
	const DataSetIpi * const dataSet,
	const uint32_t profileOffset,
	Property * const property,
	Exception * const exception) {
	Item profileItem;
	Profile *profile = NULL;
	bool valueFound = false;

	if (profileOffset != NULL_PROFILE_OFFSET) {
		DataReset(&profileItem.data);
		const CollectionKey profileKey = {
			profileOffset,
			CollectionKeyType_Profile,
		};
		profile = (Profile*)dataSet->profiles->get(
			dataSet->profiles,
			&profileKey,
			&profileItem,
			exception);
		// If profile is found
		if (profile != NULL && EXCEPTION_OKAY) {
			ProfileIterateValuesForProperty(
				dataSet->values,
				profile,
				property,
				&valueFound,
				visitProfilePropertyValue,
				exception);
			COLLECTION_RELEASE(dataSet->profiles, &profileItem);
		}
	}
	return valueFound;
}

static bool resultHasValidPropertyValueOffset(
	fiftyoneDegreesResultsIpi* const results,
	fiftyoneDegreesResultIpi* const result,
	Property * const property,
	fiftyoneDegreesException* const exception) {
	bool hasValidOffset = false;
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	Item item;
	DataReset(&item.data);

	// We will only execute this step if successfully obtained the
	// profile groups offset from the previous step
	if (result->graphResult.rawOffset != NULL_PROFILE_OFFSET) {
		if (!result->graphResult.isGroupOffset) {
			Item profileItem;
			const Profile* const profile = getResultProfile(
				results,
				result,
				&profileItem,
				exception);
			if (profile != NULL && EXCEPTION_OKAY) {
				ProfileIterateValuesForProperty(
					dataSet->values,
					profile,
					property,
					&hasValidOffset,
					visitProfilePropertyValue,
					exception);
				releaseResultProfile(dataSet, &profileItem);
			}
		} else {
			const Collection * const profileGroups = dataSet->profileGroups;
			for (uint32_t totalWeight = 0,
				nextOffset = result->graphResult.offset;
				(!hasValidOffset) && (totalWeight < FULL_RAW_WEIGHTING) && EXCEPTION_OKAY;
				++nextOffset) {
				const CollectionKey profileGroupKey = {
					nextOffset,
					&CollectionKeyType_OffsetPercentage,
				};
				const offsetPercentage* const nextWeightedProfileOffset = (const offsetPercentage*)profileGroups->get(
					profileGroups,
					&profileGroupKey,
					&item,
					exception);
				if (!(nextWeightedProfileOffset && EXCEPTION_OKAY)) {
					break;
				}
				totalWeight += nextWeightedProfileOffset->rawWeighting;
				if (totalWeight <= FULL_RAW_WEIGHTING) {
					hasValidOffset = profileHasValidPropertyValue(
						dataSet,
						nextWeightedProfileOffset->offset,
						property,
						exception);
				} else {
					EXCEPTION_SET(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA);
				}
				COLLECTION_RELEASE(profileGroups, &item);
			}
		}
	}
	return hasValidOffset;
}

static bool resultsGetHasValidPropertyValueOffset(
	fiftyoneDegreesResultsIpi* const results,
	const int requiredPropertyIndex,
	fiftyoneDegreesException* const exception) {
	bool hasValidOffset = false;
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;

	// Return the outcome of a previous walk over the same results for the
	// property if one was recorded.
	const bool canUseCache = results->hasValuesCache != NULL &&
		requiredPropertyIndex >= 0 &&
		(uint32_t)requiredPropertyIndex < results->hasValuesCacheCount;
	if (canUseCache) {
		const byte cached = results->hasValuesCache[requiredPropertyIndex];
		if (cached != IPI_HAS_VALUES_UNKNOWN) {
			return cached == IPI_HAS_VALUES_TRUE;
		}
	}

	// Work out the property index from the required property index.
	const int32_t propertyIndex = PropertiesGetPropertyIndexFromRequiredIndex(
		dataSet->b.b.available,
		requiredPropertyIndex);

	if (propertyIndex >= 0) {
		// Set the property that will be available in the results structure.
		// This may also be needed to work out which of a selection of results
		// are used to obtain the values.
		Property * const property = PropertyGet(
			dataSet->properties,
			propertyIndex,
			&results->propertyItem,
			exception);

		if (property != NULL && EXCEPTION_OKAY) {
			// Only the result for the component that owns the property can
			// hold values for it. Profiles only carry values for properties
			// of their own component, so the results for other components
			// are skipped.
			for (uint32_t i = 0;
				i < results->count && !hasValidOffset && EXCEPTION_OKAY;
				i++) {
				if (results->items[i].componentIndex ==
					(int32_t)property->componentIndex) {
					hasValidOffset = resultHasValidPropertyValueOffset(
						results,
						&results->items[i],
						property,
						exception);
				}
			}

			// Record the outcome of the walk so that later has values or
			// no value reason calls for the same property and results do
			// not need to repeat it.
			if (EXCEPTION_OKAY && canUseCache) {
				results->hasValuesCache[requiredPropertyIndex] =
					hasValidOffset ?
					IPI_HAS_VALUES_TRUE : IPI_HAS_VALUES_FALSE;
			}
		}
	}
	return hasValidOffset;
}

bool fiftyoneDegreesResultsIpiGetHasValues(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception) {
	const DataSetIpi *dataSet = (DataSetIpi*)results->b.dataSet;
	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	if (requiredPropertyIndex < 0 ||
		requiredPropertyIndex >= (int)dataSet->b.b.available->count) {
		// The property index is not valid.
		return false;
	}

	if (results->count == 0)
		// There is no result
		return false;

	// Check the result for the component the property belongs to. The
	// outcome of a previous walk for the property is used if available.
	const bool hasValidOffset = resultsGetHasValidPropertyValueOffset(
		results,
		requiredPropertyIndex,
		exception);
	if (EXCEPTION_FAILED) {
		return false;
	}
	if (hasValidOffset) {
		return true;
	}
	const uint32_t propertyIndex = PropertiesGetPropertyIndexFromRequiredIndex(
		dataSet->b.b.available,
		requiredPropertyIndex);

	if (propertyIndex >= 0) {
		Property *property = PropertyGet(
			dataSet->properties,
			propertyIndex,
			&results->propertyItem,
			exception);
		if (property != NULL && EXCEPTION_OKAY) {
			if (property->defaultValueIndex != UINT32_MAX &&
				property->isMandatory) {
				// Although there is no values, the property is mandatory,
				// and there is a default value which will be used. So
				// return true.
				return true;
			}
		}
	}
	return false;
}

fiftyoneDegreesResultsNoValueReason fiftyoneDegreesResultsIpiGetNoValueReason(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception) {
	const DataSetIpi *dataSet = (DataSetIpi*)results->b.dataSet;
	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	if (requiredPropertyIndex < 0 ||
		requiredPropertyIndex >= (int)dataSet->b.b.available->count) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY;
	}

	if (results->count == 0) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS;
	}

	// Check the result for the component the property belongs to. The
	// outcome of a previous walk for the property is used if available.
	const bool hasValidOffset = resultsGetHasValidPropertyValueOffset(
		results,
		requiredPropertyIndex,
		exception);
	if (EXCEPTION_FAILED) {
		return false;
	}
	if (hasValidOffset) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN;
	}
	if (EXCEPTION_OKAY) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE;
	}

	return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN;
}

const char* fiftyoneDegreesResultsIpiGetNoValueReasonMessage(
	fiftyoneDegreesResultsNoValueReason reason) {
	switch (reason) {
	case FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS:
		return "The results are empty. This is probably because we don't "
			"have this data in our database.";
	case FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE:
		return "The results contained a null profile for the component which "
			"the required property belongs to.";
	case FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY:
		return "The requested property does not exist, or is not a required property";
	case FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN:
	default:
		return "The reason for missing values is unknown.";
	}
}

static void pushValues(
	const WeightedItem * const weightedItem,
	const uint32_t count,
	StringBuilder * const builder,
	const char * const separator,
	PropertyValueType storedValueType,
	const uint8_t decimalPlaces,
	Exception * const exception) {

	const size_t sepLen = strlen(separator);

	// Loop through the values adding them to the string buffer.
	for (uint32_t i = 0; i < count;  i++) {
		// Append the separator
		if (i) {
			StringBuilderAddChars(builder, separator, sepLen);
		}

		// Add the opening quote
		StringBuilderAddChar(builder, '"');

		// Get the string for the value index.
		const StoredBinaryValue * const binaryValue =
			(const StoredBinaryValue*)weightedItem[i].item.data.ptr;

		// Add the string to the output buffer recording the number
		// of characters added.
		StringBuilderAddStringValue(
			builder,
			binaryValue,
			storedValueType,
			decimalPlaces,
			exception);

		// Add the closing quote
		StringBuilderAddChar(builder, '"');
		StringBuilderAddChar(builder, ':');
		StringBuilderAddDouble(
			builder,
			(double)weightedItem[i].rawWeighting / (double)FIFTYONE_DEGREES_WEIGHTED_ITEM_MAX_WEIGHT,
			decimalPlaces);
	}
}

static void fiftyoneDegreesResultsIpiGetValuesStringInternal(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	StringBuilder * const builder,
	const char* separator,
	fiftyoneDegreesException* exception) {
	const WeightedItem *weightedItem;
	Item propertyItem;
	Property *property;
	const DataSetIpi *dataSet = (DataSetIpi *)results->b.dataSet;

	const int propertyIndex = PropertiesGetPropertyIndexFromRequiredIndex(
		dataSet->b.b.available,
		requiredPropertyIndex);

	if (propertyIndex >= 0) {
		PropertyValueType const storedValueType = PropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			propertyIndex,
			exception);
		if (EXCEPTION_FAILED) {
			return;
		}
		DataReset(&propertyItem.data);
		const CollectionKey propertyKey = {
			propertyIndex,
			CollectionKeyType_Property,
		};
		property = (Property*)dataSet->properties->get(
			dataSet->properties,
			&propertyKey,
			&propertyItem,
			exception);
		if (property != NULL && EXCEPTION_OKAY) {
			if (requiredPropertyIndex >= 0) {
				weightedItem = fiftyoneDegreesResultsIpiGetValues(
					results,
					requiredPropertyIndex,
					exception);
				if (weightedItem != NULL && EXCEPTION_OKAY) {
					pushValues(
						weightedItem,
						results->values.count,
						builder,
						separator,
						storedValueType,
						DefaultWktDecimalPlaces,
						exception);
				}
			}
			COLLECTION_RELEASE(dataSet->properties, &propertyItem);
		}
	}
}

void fiftyoneDegreesResultsIpiAddValuesString(
	fiftyoneDegreesResultsIpi* results,
	const char* propertyName,
	fiftyoneDegreesStringBuilder *builder,
	const char* separator,
	fiftyoneDegreesException* exception) {
	const DataSetIpi * const dataSet = (DataSetIpi *)results->b.dataSet;
	const int requiredPropertyIndex = PropertiesGetRequiredPropertyIndexFromName(
		dataSet->b.b.available,
		propertyName);

	if (requiredPropertyIndex >= 0) {
		fiftyoneDegreesResultsIpiGetValuesStringInternal(
			results,
			requiredPropertyIndex,
			builder,
			separator,
			exception);
	}
}

size_t fiftyoneDegreesResultsIpiGetValuesString(
	fiftyoneDegreesResultsIpi* results,
	const char* propertyName,
	char* buffer,
	size_t bufferLength,
	const char* separator,
	fiftyoneDegreesException* exception) {

	StringBuilder builder = { buffer, bufferLength };
	StringBuilderInit(&builder);

	fiftyoneDegreesResultsIpiAddValuesString(
		results,
		propertyName,
		&builder,
		separator,
		exception);

	StringBuilderComplete(&builder);

	return builder.added;
}

size_t fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
	fiftyoneDegreesResultsIpi* results,
	const int requiredPropertyIndex,
	char* buffer,
	size_t bufferLength,
	const char* separator,
	fiftyoneDegreesException* exception) {

	StringBuilder builder = { buffer, bufferLength };
	StringBuilderInit(&builder);

	fiftyoneDegreesResultsIpiGetValuesStringInternal(
		results,
		requiredPropertyIndex,
		&builder,
		separator,
		exception);

	StringBuilderComplete(&builder);

	return builder.added;
}

/*
 * Supporting Macros to printout the NetworkId
 */
#define PRINT_PROFILE_SEP(d,b,s,f) printProfileSeparator(&d, (b - d) + s, f)
#define PRINT_PROFILE_ID(d,b,s,f,v,p) printProfileId(&d, (b - d) + s, f, v, p)
#define PRINT_NULL_PROFILE_ID(d,b,s,p) PRINT_PROFILE_ID(d, b, s, "%i:%f", 0, p)

size_t fiftyoneDegreesIpiGetIpAddressAsString(
	const fiftyoneDegreesCollectionItem * const item,
	const fiftyoneDegreesIpType type,
	char * const buffer,
	const uint32_t bufferLength,
	fiftyoneDegreesException * const exception) {

	StringBuilder builder = { buffer, bufferLength };
	StringBuilderInit(&builder);

	StringBuilderAddIpAddress(
		&builder,
		(const VarLengthByteArray *)item->data.ptr,
		type,
		exception);

	return builder.added;
}

uint32_t fiftyoneDegreesIpiIterateProfilesForPropertyAndValue(
	fiftyoneDegreesResourceManager* manager,
	const char* propertyName,
	const char* valueName,
	void* state,
	fiftyoneDegreesProfileIterateMethod callback,
	fiftyoneDegreesException* exception) {
	uint32_t count = 0;
	DataSetIpi* dataSet = DataSetIpiGet(manager);
	count = ProfileIterateProfilesForPropertyWithTypeAndValueAndOffsetExtractor(
		dataSet->strings,
		dataSet->properties,
		dataSet->propertyTypes,
		dataSet->values,
		dataSet->profiles,
		dataSet->profileOffsets,
		ProfileOffsetAsPureOffset,
		propertyName,
		valueName,
		state,
		callback,
		exception);
	DataSetIpiRelease(dataSet);
	return count;
}
