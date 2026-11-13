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
#include "../common/threading.h"
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

/* A concurrency of 0 is one operation for each processor. */
#define DEFAULT_CONCURRENCY(c,t) if ((c)->t.concurrency == 0) { \
(c)->t.concurrency = ThreadingGetProcessorCount(); }

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
	{0,0,0}, // Graph
	false, // LoadRequiredPropertiesOnly
	true // DefaultValuesInMemory
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
	{ true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Graph
	true, // LoadRequiredPropertiesOnly
	true // DefaultValuesInMemory
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
	{ false, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, // Graph
	false, // LoadRequiredPropertiesOnly
	true // DefaultValuesInMemory
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
{ FIFTYONE_DEGREES_IP_GRAPH_LOADED, FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Graph */ \
false, /* LoadRequiredPropertiesOnly */ \
true /* DefaultValuesInMemory */

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
	{ FIFTYONE_DEGREES_IP_GRAPH_LOADED, FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE, FIFTYONE_DEGREES_CACHE_CONCURRENCY }, /* Graph */
	false, /* LoadRequiredPropertiesOnly */
	true /* DefaultValuesInMemory */
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
		// Record the component, so that it is known without reading the
		// property again.
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
	if (dataSet->graphsArray == NULL) {
		// Graphs the memory is too short to hold leave the position short of
		// the end, so their failure is returned first.
		return getFailureStatus(CORRUPT_DATA, exception);
	}

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
	ConfigIpi *copy = (ConfigIpi*)&dataSet->config;
	DEFAULT_CONCURRENCY(copy, strings);
	DEFAULT_CONCURRENCY(copy, components);
	DEFAULT_CONCURRENCY(copy, maps);
	DEFAULT_CONCURRENCY(copy, properties);
	DEFAULT_CONCURRENCY(copy, values);
	DEFAULT_CONCURRENCY(copy, profiles);
	DEFAULT_CONCURRENCY(copy, graphs);
	DEFAULT_CONCURRENCY(copy, profileGroups);
	DEFAULT_CONCURRENCY(copy, profileOffsets);
	DEFAULT_CONCURRENCY(copy, propertyTypes);
	DEFAULT_CONCURRENCY(copy, graph);
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
 * With loadRequiredPropertiesOnly, a caller that names some of the properties
 * only reads part of the data file. Only that part of the strings, values,
 * profiles, profile groups and profile offsets is held in memory, with the
 * graphs of the components named, and the rest reads from the file. Each
 * stretch held is made of whole records, so a lookup that reads the named
 * properties reads none of those collections from the file, and anything else
 * still gets the same answer, more slowly.
 *
 * With defaultValuesInMemory, the default value of each required property is
 * held, with what reading it takes from the strings and the property types. A
 * lookup reads the default for each mandatory property its profile has no
 * value for.
 */

/** The number of values a component id, which is a byte, can have. */
#define HELD_COMPONENT_IDS 256

/** What creating a collection that is held in full needs. */
typedef struct held_create_t {
	DataSetIpi* dataSet;
	FILE* file;
	CollectionFileRead read;
	byte shift;
} heldCreateState;

/**
 * Creates the collection loaded into memory that replaces one that is held in
 * full.
 */
static StatusCode heldCreateInMemory(
	void* state,
	const fiftyoneDegreesCollectionConfig* config,
	CollectionHeader header,
	Collection** collection) {
	const heldCreateState* const create = (const heldCreateState*)state;
#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
	*collection = fiftyoneDegreesCollectionCreateFromFileWithOffsetShift(
		create->file,
		&create->dataSet->b.b.filePool,
		config,
		header,
		create->read,
		create->shift);
#else
	*collection = CollectionCreateFromFile(
		create->file,
		&create->dataSet->b.b.filePool,
		config,
		header,
		create->read);
#endif
	return *collection == NULL ?
		getCollectionFileStatus(create->dataSet, header, create->shift) :
		SUCCESS;
}

/**
 * Holds the ranges of a collection that reads from the file. See
 * CollectionHeldApply.
 */
static StatusCode heldApply(
	DataSetIpi* dataSet,
	FILE* file,
	Collection** collection,
	CollectionHeader header,
	CollectionFileRead read,
	const byte shift,
	const bool replaceable,
	CollectionHeldRange* ranges,
	const uint32_t rangeCount) {
	heldCreateState create;
	create.dataSet = dataSet;
	create.file = file;
	create.read = read;
	create.shift = shift;
	return CollectionHeldApply(
		collection,
		file,
		header,
		shift,
		replaceable,
		ranges,
		rangeCount,
		heldCreateInMemory,
		&create);
}

/**
 * What is worked out before anything is held.
 */
typedef struct held_plan_t {
	byte* buffer; /* Used to scan records */
	CollectionHeldStrings strings; /* The strings the required properties read */
	PropertyValueType scanType; /* How the strings being scanned are stored */
	CollectionHeldRange* values; /* The stretches of the values to hold */
	uint32_t valuesCount; /* Number of stretches of the values */
	CollectionHeldRange* profileOffsets; /* The stretches of the profile offsets */
	uint32_t profileOffsetsCount; /* Number of stretches of profile offsets */
	CollectionHeldRange* profileGroups; /* The stretches of the profile groups */
	uint32_t profileGroupsCount; /* Number of stretches of profile groups */
	CollectionHeldRange* profiles; /* The stretches of the profiles to hold */
	uint32_t profilesCount; /* Number of stretches of the profiles */
} heldPlan;

static void heldFreeIfSet(void* memory) {
	if (memory != NULL) {
		Free(memory);
	}
}

static void heldPlanFree(heldPlan* plan) {
	heldFreeIfSet(plan->buffer);
	CollectionHeldStringsFree(&plan->strings);
	heldFreeIfSet(plan->values);
	heldFreeIfSet(plan->profileOffsets);
	heldFreeIfSet(plan->profileGroups);
	heldFreeIfSet(plan->profiles);
}

static void heldPlanScanValue(void* state, const byte* record) {
	heldPlan* const plan = (heldPlan*)state;
	CollectionHeldStringsAdd(
		&plan->strings,
		(uint32_t)((const Value*)record)->nameOffset,
		(uint32_t)plan->scanType);
}

/**
 * Creates the graphs, with those of a component that has no required property
 * reading from the file.
 */
static StatusCode heldGraphsCreate(
	DataSetIpi* dataSet,
	FILE* file,
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
	return SUCCESS;
}

/**
 * Records the strings of a stretch of values and plans to hold it.
 */
static StatusCode heldPlanValues(
	const DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	const uint32_t first,
	const uint32_t count) {
	const uint32_t valueBytes = dataSet->values->elementSize;
	CollectionHeldRangeAdd(plan->values, &plan->valuesCount, first, first + count);
	return CollectionHeldScan(
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
 * planned on its own if it is not in the range of its property.
 */
static StatusCode heldPlanProperties(
	DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	const PropertiesAvailable* const available = dataSet->b.b.available;
	const uint32_t valueCount = dataSet->values->count;
	plan->values = (CollectionHeldRange*)Malloc(
		sizeof(CollectionHeldRange) * available->count * 2);
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

static uint32_t heldStringEndMethod(
	void* state,
	const uint32_t offset,
	const uint32_t tag,
	Exception* exception) {
	return heldStringEnd(
		(const DataSetIpi*)state,
		offset,
		(PropertyValueType)tag,
		exception);
}

/**
 * Works out the stretches of the strings to hold from the strings recorded,
 * which leaves on file any stretch long enough that holds no required string.
 */
static StatusCode heldPlanStrings(
	const DataSetIpi* dataSet,
	heldPlan* plan,
	Exception* exception) {
	return CollectionHeldStringsPlan(
		&plan->strings,
		heldStringEndMethod,
		(void*)dataSet,
		exception);
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
 * Plans for the profile offsets, the profile groups and the profiles of each
 * component with a required property. A component's profiles are next to each
 * other in the file, so they are held from the first to the end of the last.
 */
static StatusCode heldPlanComponents(
	const DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan,
	Exception* exception) {
	const uint32_t size = dataSet->header.profiles.length;
	const uint64_t offsetsStart =
		(uint64_t)dataSet->header.profileOffsets.startPosition;
	const uint64_t groupsStart =
		(uint64_t)dataSet->header.profileGroups.startPosition;
	const uint32_t graphCount = dataSet->graphsArray->count;
	plan->profileOffsets = (CollectionHeldRange*)Malloc(sizeof(CollectionHeldRange) * graphCount);
	plan->profileGroups = (CollectionHeldRange*)Malloc(sizeof(CollectionHeldRange) * graphCount);
	plan->profiles = (CollectionHeldRange*)Malloc(
		sizeof(CollectionHeldRange) * dataSet->componentsList.count);
	if (plan->profileOffsets == NULL ||
		plan->profileGroups == NULL ||
		plan->profiles == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
		const Component* const component = COMPONENT(dataSet, c);
		if (component == NULL || dataSet->componentsAvailable[c] == false) {
			continue;
		}
		heldProfileRun run = { UINT32_MAX, 0 };
		for (uint32_t g = 0; g < graphCount; g++) {
			const fiftyoneDegreesIpiCgInfo* const info =
				&dataSet->graphsArray->items[g].info;
			if (info->componentId != component->componentId) {
				continue;
			}
			CollectionHeldRangeAddWithin(
				plan->profileOffsets,
				&plan->profileOffsetsCount,
				info->firstProfileIndex,
				info->profileCount,
				dataSet->header.profileOffsets.count);
			CollectionHeldRangeAddWithin(
				plan->profileGroups,
				&plan->profileGroupsCount,
				info->firstProfileGroupIndex,
				info->profileGroupCount,
				dataSet->header.profileGroups.count);
			StatusCode status = CollectionHeldScan(
				file,
				plan->buffer,
				offsetsStart +
					(uint64_t)info->firstProfileIndex * sizeof(uint32_t),
				info->profileCount,
				sizeof(uint32_t),
				heldProfileRunScanOffset,
				&run);
			if (status == SUCCESS) {
				status = CollectionHeldScan(
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
			CollectionHeldRangeAdd(
				plan->profiles,
				&plan->profilesCount,
				run.first,
				run.last + units);
		}
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
	StatusCode status = heldGraphsCreate(dataSet, file, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	plan->buffer = (byte*)Malloc(
		FIFTYONE_DEGREES_COLLECTION_HELD_SCAN_BUFFER_BYTES);
	status = CollectionHeldStringsInit(
		&plan->strings,
		dataSet->header.strings.length);
	if (plan->buffer == NULL || status != SUCCESS) {
		return INSUFFICIENT_MEMORY;
	}
	status = heldPlanProperties(dataSet, file, plan, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	status = heldPlanStrings(dataSet, plan, exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		return status;
	}
	return heldPlanComponents(dataSet, file, plan, exception);
}

/**
 * Holds what the plan says to hold. Anything allocated is freed with the data
 * set if this fails.
 */
static StatusCode heldPlanApply(
	DataSetIpi* dataSet,
	FILE* file,
	heldPlan* plan) {
	// The names of the required properties are read from the strings while the
	// data set is created and released through them when it is freed, so the
	// strings stay.
	StatusCode status = heldApply(
		dataSet,
		file,
		&dataSet->strings,
		dataSet->header.strings,
		fiftyoneDegreesStoredBinaryValueRead,
		0,
		false,
		plan->strings.ranges,
		plan->strings.count);
	if (status == SUCCESS) {
		status = heldApply(
			dataSet,
			file,
			&dataSet->values,
			dataSet->header.values,
			CollectionReadFileFixed,
			0,
			true,
			plan->values,
			plan->valuesCount);
	}
	if (status == SUCCESS) {
		status = heldApply(
			dataSet,
			file,
			&dataSet->profileOffsets,
			dataSet->header.profileOffsets,
			CollectionReadFileFixed,
			0,
			true,
			plan->profileOffsets,
			plan->profileOffsetsCount);
	}
	if (status == SUCCESS) {
		status = heldApply(
			dataSet,
			file,
			&dataSet->profileGroups,
			dataSet->header.profileGroups,
			CollectionReadFileFixed,
			0,
			true,
			plan->profileGroups,
			plan->profileGroupsCount);
	}
	if (status == SUCCESS) {
		status = heldApply(
			dataSet,
			file,
			&dataSet->profiles,
			dataSet->header.profiles,
			fiftyoneDegreesProfileReadFromFile,
			getProfilesFileShift(dataSet),
			true,
			plan->profiles,
			plan->profilesCount);
	}
	return status;
}

/**
 * Creates the graphs and holds in memory what the required properties read.
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
		// The memory used to work out the plan goes before anything is held.
		heldFreeIfSet(plan.buffer);
		plan.buffer = NULL;
		CollectionHeldStringsFreeOffsets(&plan.strings);
		status = heldPlanApply(dataSet, file, &plan);
	}
	heldPlanFree(&plan);
	fclose(file);
	if (status == SUCCESS && EXCEPTION_FAILED) {
		status = COLLECTION_FAILURE;
	}
	return status;
}

/**
 * What holding the default values holds, from each collection not loaded into
 * memory.
 */
typedef struct held_defaults_t {
	CollectionHeldRange* values; /* The default value records */
	uint32_t valuesCount; /* Number of ranges of the values */
	CollectionHeldRange* strings; /* What the default values read from the strings */
	uint32_t stringsCount; /* Number of ranges of the strings */
	CollectionHeldRange* types; /* The property types the default values read */
	uint32_t typesCount; /* Number of ranges of the property types */
} heldDefaults;

/**
 * Plans to hold the default value of a property, with what reading it takes
 * from the strings and the property types, from each of those collections not
 * loaded into memory. Every read follows what a lookup reads for a default.
 */
static StatusCode heldDefaultsPlan(
	const DataSetIpi* dataSet,
	const uint32_t propertyIndex,
	heldDefaults* defaults,
	Exception* exception) {
	const ConfigIpi* const config = &dataSet->config;
	Item item;
	DataReset(&item.data);
	const Property* const property = PropertyGet(
		dataSet->properties,
		propertyIndex,
		&item,
		exception);
	if (property == NULL || EXCEPTION_FAILED) {
		if (property != NULL) {
			COLLECTION_RELEASE(dataSet->properties, &item);
		}
		return COLLECTION_FAILURE;
	}
	const uint32_t index = property->defaultValueIndex;
	COLLECTION_RELEASE(dataSet->properties, &item);
	if (index >= dataSet->values->count) {
		return SUCCESS;
	}
	if (config->values.loaded == false) {
		CollectionHeldRangeAdd(
			defaults->values,
			&defaults->valuesCount,
			index,
			index + 1);
	}
	if (config->strings.loaded && config->propertyTypes.loaded) {
		return SUCCESS;
	}
	DataReset(&item.data);
	const Value* const value = ValueGet(
		dataSet->values,
		index,
		&item,
		exception);
	if (value == NULL || EXCEPTION_FAILED) {
		if (value != NULL) {
			COLLECTION_RELEASE(dataSet->values, &item);
		}
		return COLLECTION_FAILURE;
	}
	const uint32_t typeIndex = (uint32_t)value->propertyIndex;
	const uint32_t offset = (uint32_t)value->nameOffset;
	COLLECTION_RELEASE(dataSet->values, &item);
	if (typeIndex >= dataSet->header.propertyTypes.count) {
		return SUCCESS;
	}
	if (config->propertyTypes.loaded == false) {
		CollectionHeldRangeAdd(
			defaults->types,
			&defaults->typesCount,
			typeIndex,
			typeIndex + 1);
	}
	if (config->strings.loaded || offset >= dataSet->header.strings.length) {
		return SUCCESS;
	}
	const PropertyValueType type = PropertyGetStoredTypeByIndex(
		dataSet->propertyTypes,
		typeIndex,
		exception);
	if (EXCEPTION_FAILED) {
		return COLLECTION_FAILURE;
	}
	DataReset(&item.data);
	const StoredBinaryValue* const stored = StoredBinaryValueGet(
		dataSet->strings,
		offset,
		type,
		&item,
		exception);
	if (stored == NULL || EXCEPTION_FAILED) {
		if (stored != NULL) {
			COLLECTION_RELEASE(dataSet->strings, &item);
		}
		return COLLECTION_FAILURE;
	}
	CollectionHeldRangeAdd(
		defaults->strings,
		&defaults->stringsCount,
		offset,
		offset + item.data.used);
	COLLECTION_RELEASE(dataSet->strings, &item);
	return SUCCESS;
}

/**
 * Holds the default value of each required property, with what reading it
 * takes from the strings and the property types, so that reading one takes no
 * lock and reads nothing from the file. Nothing is copied from a collection
 * loaded into memory, so this does nothing when all three are. Anything
 * allocated is freed with the data set if this fails.
 */
static StatusCode heldDefaultsInit(DataSetIpi* dataSet, Exception* exception) {
	const PropertiesAvailable* const available = dataSet->b.b.available;
	const ConfigIpi* const config = &dataSet->config;
	heldDefaults defaults;
	FILE* file;
	if (available->count == 0 ||
		(config->values.loaded &&
			config->strings.loaded &&
			config->propertyTypes.loaded)) {
		return SUCCESS;
	}
	CollectionHeldRange* const ranges = (CollectionHeldRange*)Malloc(
		sizeof(CollectionHeldRange) * (size_t)available->count * 3);
	if (ranges == NULL) {
		return INSUFFICIENT_MEMORY;
	}
	memset(&defaults, 0, sizeof(defaults));
	defaults.values = ranges;
	defaults.strings = ranges + available->count;
	defaults.types = ranges + (size_t)available->count * 2;
	StatusCode status = SUCCESS;
	for (uint32_t i = 0; i < available->count && status == SUCCESS; i++) {
		status = heldDefaultsPlan(
			dataSet,
			available->items[i].propertyIndex,
			&defaults,
			exception);
	}
	const uint32_t rangeCount =
		defaults.valuesCount + defaults.stringsCount + defaults.typesCount;
	if (status == SUCCESS && rangeCount > 0) {
		status = FileOpen(dataSet->b.b.fileName, &file);
		if (status == SUCCESS) {
			status = CollectionHeldCreate(
				&dataSet->values,
				file,
				(uint64_t)dataSet->header.values.startPosition,
				dataSet->values->elementSize,
				defaults.values,
				defaults.valuesCount);
			if (status == SUCCESS) {
				status = CollectionHeldCreate(
					&dataSet->strings,
					file,
					(uint64_t)dataSet->header.strings.startPosition,
					1,
					defaults.strings,
					defaults.stringsCount);
			}
			if (status == SUCCESS) {
				status = CollectionHeldCreate(
					&dataSet->propertyTypes,
					file,
					(uint64_t)dataSet->header.propertyTypes.startPosition,
					dataSet->propertyTypes->elementSize,
					defaults.types,
					defaults.typesCount);
			}
			fclose(file);
		}
	}
	Free(ranges);
	if (status == SUCCESS && EXCEPTION_FAILED) {
		status = COLLECTION_FAILURE;
	}
	return status;
}

/**
 * True when loadRequiredPropertiesOnly is set, the data set reads the file,
 * and the caller has named fewer properties than the data file has.
 */
static bool heldIsWanted(
	const DataSetIpi* dataSet,
	const PropertiesRequired* properties) {
	if (dataSet->config.loadRequiredPropertiesOnly == false ||
		dataSet->config.b.allInMemory) {
		return false;
	}
	return PropertiesRequiredNamesFewerThan(
		properties,
		dataSet->header.properties.count);
}

/**
 * Gets the configuration to create a collection with, which reads from the
 * file when what the required properties read is to be held, as they are not
 * known yet.
 */
static const fiftyoneDegreesCollectionConfig* heldConfig(
	const fiftyoneDegreesCollectionConfig* config,
	const bool hold) {
	static const fiftyoneDegreesCollectionConfig readFromFile = { false, 0, 0 };
	return hold ? &readFromFile : config;
}

/**
 * Checks that each collection the hold reads from lies within the file, as one
 * loaded into memory is checked when it is loaded.
 */
static StatusCode heldFileStatus(const DataSetIpi* dataSet) {
	const DataSetIpiHeader* const header = &dataSet->header;
	const bool beyond =
		getCollectionFileStatus(dataSet, header->strings, 0) == CORRUPT_DATA ||
		getCollectionFileStatus(dataSet, header->values, 0) == CORRUPT_DATA ||
		getCollectionFileStatus(dataSet, header->profileGroups, 0) ==
			CORRUPT_DATA ||
		getCollectionFileStatus(dataSet, header->profileOffsets, 0) ==
			CORRUPT_DATA ||
		getCollectionFileStatus(
			dataSet,
			header->profiles,
			getProfilesFileShift(dataSet)) == CORRUPT_DATA;
	return beyond ? CORRUPT_DATA : SUCCESS;
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

	// Create the strings collection. When only what the required properties
	// read will be held, the collections it is held from read from the file
	// until the required properties are known.
	const bool hold = heldIsWanted(dataSet, properties);
	if (hold) {
		status = heldFileStatus(dataSet);
		if (status != SUCCESS) {
			return status;
		}
	}
	const uint32_t stringsCount = dataSet->header.strings.count;
	*(uint32_t*)(&dataSet->header.strings.count) = 0;
	COLLECTION_CREATE_FILE_WITH_CONFIG(
		strings,
		fiftyoneDegreesStoredBinaryValueRead,
		heldConfig(&dataSet->config.strings, hold));
	*(uint32_t*)(&dataSet->header.strings.count) = stringsCount;

	// Override the header count so that the variable collection can work.
	const uint32_t componentCount = dataSet->header.components.count;
	*(uint32_t*)(&dataSet->header.components.count) = 0;
	COLLECTION_CREATE_FILE(components, fiftyoneDegreesComponentReadFromFile);
	*(uint32_t*)(&dataSet->header.components.count) = componentCount;

	COLLECTION_CREATE_FILE(maps, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE(properties, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE_WITH_CONFIG(
		values,
		CollectionReadFileFixed,
		heldConfig(&dataSet->config.values, hold));

	status = createProfilesFromFile(
		dataSet,
		file,
		heldConfig(&dataSet->config.profiles, hold));
	if (status != SUCCESS) {
		return status;
	}

	COLLECTION_CREATE_FILE(graphs, CollectionReadFileFixed);

	COLLECTION_CREATE_FILE_WITH_CONFIG(
		profileGroups,
		CollectionReadFileFixed,
		heldConfig(&dataSet->config.profileGroups, hold));
	COLLECTION_CREATE_FILE(propertyTypes, CollectionReadFileFixed);
	COLLECTION_CREATE_FILE_WITH_CONFIG(
		profileOffsets,
		CollectionReadFileFixed,
		heldConfig(&dataSet->config.profileOffsets, hold));

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
		if (dataSet->graphsArray == NULL) {
			return getFailureStatus(CORRUPT_DATA, exception);
		}
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
	else if (dataSet->config.defaultValuesInMemory &&
		dataSet->config.b.allInMemory == false) {
		status = heldDefaultsInit(dataSet, exception);
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
 * True if a header before the one at the index has the same name.
 */
static bool isHeaderNameRepeated(Headers *headers, uint32_t index) {
	uint32_t i;
	for (i = 0; i < index; i++) {
		if (headers->items[i].name != NULL &&
			strcmp(headers->items[i].name, headers->items[index].name) == 0) {
			return true;
		}
	}
	return false;
}

size_t fiftyoneDegreesIpiGetEvidenceKeys(
	fiftyoneDegreesDataSetIpi *dataSet,
	char *buffer,
	size_t length,
	const char *separator) {
	static const char * const prefixes[] = { "query.", "server." };
	uint32_t i, p;
	size_t added;
	const char *name;
	Headers * const headers = dataSet->b.b.uniqueHeaders;
	const uint32_t count = headers == NULL ? 0 : headers->count;
	const size_t separatorLength = separator == NULL ? 0 : strlen(separator);
	StringBuilder builder = { buffer, buffer == NULL ? 0 : length };
	StringBuilderInit(&builder);
	for (i = 0; i < count; i++) {
		name = headers->items[i].name;

		// The prefixes differ from their first character, so a header gives
		// keys already written only when an earlier header has its name.
		if (name == NULL || isHeaderNameRepeated(headers, i)) {
			continue;
		}
		for (p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); p++) {
			if (builder.added > 0) {
				StringBuilderAddChars(&builder, separator, separatorLength);
			}
			StringBuilderAddChars(&builder, prefixes[p], strlen(prefixes[p]));
			StringBuilderAddChars(&builder, name, strlen(name));
		}
	}
	added = builder.added;
	StringBuilderComplete(&builder);
	return added;
}

/**
 * Sets the date to the next update date in the header when next is true, or
 * to the published date otherwise, if that date is valid, and to zeros if not,
 * so a caller that ignores the result still never reads an invalid date.
 */
static bool getHeaderDate(
	ResourceManager *manager,
	bool next,
	fiftyoneDegreesDate *date) {
	DataSetIpi *dataSet = DataSetIpiGet(manager);
	if (next) {
		*date = dataSet->header.nextUpdate;
	}
	else {
		*date = dataSet->header.published;
	}
	DataSetIpiRelease(dataSet);
	if (DateIsValid(date)) {
		return true;
	}
	date->year = 0;
	date->month = 0;
	date->day = 0;
	return false;
}

bool fiftyoneDegreesIpiGetPublished(
	fiftyoneDegreesResourceManager *manager,
	fiftyoneDegreesDate *date) {
	return getHeaderDate(manager, false, date);
}

bool fiftyoneDegreesIpiGetNextUpdate(
	fiftyoneDegreesResourceManager *manager,
	fiftyoneDegreesDate *date) {
	return getHeaderDate(manager, true, date);
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

/**
 * Walks the graph of the component for the result that holds it, against the
 * data set the results hold. Called as the results are populated when the
 * caller named the properties, and otherwise the first time a property of the
 * component is read.
 */
static void resultsIpiEvaluate(
	fiftyoneDegreesResultsBase* base,
	byte componentIndex,
	Exception* exception) {
	ResultsIpi* const results =
		(ResultsIpi*)((byte*)base - offsetof(ResultsIpi, b));
	const DataSetIpi* const dataSet = (DataSetIpi*)results->b.dataSet;
	const Component* const component = COMPONENT(dataSet, componentIndex);
	if (component == NULL) {
		return;
	}
	for (uint32_t i = 0; i < results->count; i++) {
		if (results->items[i].componentIndex == (int32_t)componentIndex) {
			setResultFromIpAddress(
				&results->items[i],
				dataSet,
				component->componentId,
				exception);
		}
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

		// Initialise the results, which walk each component's graph as they
		// are populated when the caller named the properties, and otherwise
		// when one of the component's properties is first read.
		fiftyoneDegreesResultsInit(&results->b, (void*)(&dataSet->b));
		results->b.evaluate = resultsIpiEvaluate;

		// Reset the property and values list ready for first use sized for
		// a single value to be returned. Results without the memory for the
		// list are freed, and none are returned.
		results->hasValuesCache = NULL;
		results->hasValuesCacheCount = 0;
		results->keptProfiles = NULL;
		DataReset(&results->propertyItem.data);
		WeightedItemListInit(&results->values, 1, FIFTYONE_DEGREES_WEIGHTED_ITEM_LIST_DEFAULT_LOAD_FACTOR);
		if (results->values.items == NULL) {
			fiftyoneDegreesResultsIpiFree(results);
			return NULL;
		}

		// Allocate the cache used to avoid repeating the profile walks
		// which check whether a property has values for the current
		// results. If the allocation fails the cache is not used.
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

static void addResultsFromIpAddressNoChecks(
	ResultsIpi* results,
	const unsigned char* ipAddress,
	fiftyoneDegreesIpType type,
	Exception* exception) {
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	const bool walk = ResultsWalkWhenPopulated(&dataSet->b.b);
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

		// The graph is walked now when the caller named the properties, and
		// otherwise the first time a property of the component is read.
		if (walk) {
			results->b.evaluate(&results->b, (byte)componentIndex, exception);
			if (EXCEPTION_FAILED) {
				return;
			}
		}
		else {
			ResultsPendingSet(&results->b, (byte)componentIndex);
		}
	}
}

void fiftyoneDegreesResultsIpiFromIpAddress(
	fiftyoneDegreesResultsIpi* results,
	const unsigned char* ipAddress,
	size_t ipAddressLength,
	fiftyoneDegreesIpType type,
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
	ResultsPendingClear(&results->b);

	addResultsFromIpAddressNoChecks(results, ipAddress, type, exception);
}

void fiftyoneDegreesResultsIpiFromIpAddressString(
	fiftyoneDegreesResultsIpi* results,
	const char* ipAddress,
	size_t ipLength,
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
		fiftyoneDegreesResultsIpiFromIpAddress(
			results,
			ip.value,
			IPV4_LENGTH,
			IP_TYPE_IPV4,
			exception);
		break;
	case IP_TYPE_IPV6:
		fiftyoneDegreesResultsIpiFromIpAddress(
			results,
			ip.value,
			IPV6_LENGTH,
			IP_TYPE_IPV6,
			exception);
		break;
	case IP_TYPE_INVALID:
	default:
		EXCEPTION_SET(INCORRECT_IP_ADDRESS_FORMAT);
		break;
	}
}

static bool setResultsFromEvidence(
	void* state,
	EvidenceKeyValuePair* pair) {
	const stateWithUniqueHeaderIndex* indexState = (stateWithUniqueHeaderIndex*)state;
	const stateWithException* exceptionState = (stateWithException*)indexState->subState;
	ResultsIpi* results = (ResultsIpi*)exceptionState->state;
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

void fiftyoneDegreesResultsIpiFromEvidence(
	fiftyoneDegreesResultsIpi* results,
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence,
	fiftyoneDegreesException* exception) {
	stateWithException subState;
	stateWithUniqueHeaderIndex state;
	subState.state = results;
	subState.exception = exception;
	state.subState = &subState;

	if (evidence != (EvidenceKeyValuePairArray*)NULL) {
		// Reset the results data before iterating the evidence.
		results->count = 0;
		resultsIpiResetHasValuesCache(results);
		ResultsPendingClear(&results->b);

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

static const WeightedItem* resultsIpiGetValues(
	ResultsIpi* const results,
	int const availableIndex,
	Exception* const exception) {
	Property* property;
	DataSetIpi* dataSet;
	const WeightedItem* firstValue = NULL;

	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	dataSet = (DataSetIpi*)results->b.dataSet;

	// Work out the property index from the available index.
	const int propertyIndex = PropertiesGetPropertyIndexFromAvailableIndex(
		dataSet->b.b.available,
		availableIndex);

	if (propertyIndex >= 0) {
		// Set the property that will be available in the results structure. 
		// This may also be needed to work out which of a selection of results 
		// are used to obtain the values.
		property = PropertyGet(
			dataSet->properties,
			(uint32_t)propertyIndex,
			&results->propertyItem,
			exception);

		if (property != NULL && EXCEPTION_OKAY) {
			// Ensure there is a collection available to the property item so
			// that it can be freed when the results are freed.
			if (results->propertyItem.collection == NULL) {
				results->propertyItem.collection = dataSet->properties;
			}

			// The first read of a property of the component walks its graph.
			ResultsEvaluate(&results->b, property->componentIndex, exception);

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

			if (EXCEPTION_OKAY &&
				results->values.count == 0 &&
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

const fiftyoneDegreesWeightedItem* fiftyoneDegreesResultsIpiGetValues(
	fiftyoneDegreesResultsIpi* const results,
	int const requiredPropertyIndex,
	fiftyoneDegreesException* const exception) {
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	return resultsIpiGetValues(
		results,
		PropertiesGetAvailableIndex(
			dataSet->b.b.available,
			requiredPropertyIndex),
		exception);
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
	const int availableIndex,
	fiftyoneDegreesException* const exception) {
	bool hasValidOffset = false;
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;

	// Return the outcome of a previous walk over the same results for the
	// property if one was recorded.
	const bool canUseCache = results->hasValuesCache != NULL &&
		availableIndex >= 0 &&
		(uint32_t)availableIndex < results->hasValuesCacheCount;
	if (canUseCache) {
		const byte cached = results->hasValuesCache[availableIndex];
		if (cached != IPI_HAS_VALUES_UNKNOWN) {
			return cached == IPI_HAS_VALUES_TRUE;
		}
	}

	// Work out the property index from the available index.
	const int32_t propertyIndex = PropertiesGetPropertyIndexFromAvailableIndex(
		dataSet->b.b.available,
		availableIndex);

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
			// The first read of a property of the component walks its graph.
			ResultsEvaluate(&results->b, property->componentIndex, exception);

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
				results->hasValuesCache[availableIndex] =
					hasValidOffset ?
					IPI_HAS_VALUES_TRUE : IPI_HAS_VALUES_FALSE;
			}
		}
	}
	return hasValidOffset;
}

static bool resultsIpiGetHasValues(
	ResultsIpi* results,
	int availableIndex,
	Exception* exception) {
	const DataSetIpi *dataSet = (DataSetIpi*)results->b.dataSet;
	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	if (availableIndex < 0 ||
		availableIndex >= (int)dataSet->b.b.available->count) {
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
		availableIndex,
		exception);
	if (EXCEPTION_FAILED) {
		return false;
	}
	if (hasValidOffset) {
		return true;
	}
	const int propertyIndex = PropertiesGetPropertyIndexFromAvailableIndex(
		dataSet->b.b.available,
		availableIndex);

	if (propertyIndex >= 0) {
		Property *property = PropertyGet(
			dataSet->properties,
			(uint32_t)propertyIndex,
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

bool fiftyoneDegreesResultsIpiGetHasValues(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception) {
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	return resultsIpiGetHasValues(
		results,
		PropertiesGetAvailableIndex(
			dataSet->b.b.available,
			requiredPropertyIndex),
		exception);
}

static fiftyoneDegreesResultsNoValueReason resultsIpiGetNoValueReason(
	ResultsIpi* results,
	int availableIndex,
	Exception* exception) {
	const DataSetIpi *dataSet = (DataSetIpi*)results->b.dataSet;
	// Ensure any previous uses of the results to get values are released.
	resultsIpiRelease(results);

	if (availableIndex < 0 ||
		availableIndex >= (int)dataSet->b.b.available->count) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY;
	}

	if (results->count == 0) {
		return FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS;
	}

	// Check the result for the component the property belongs to. The
	// outcome of a previous walk for the property is used if available.
	const bool hasValidOffset = resultsGetHasValidPropertyValueOffset(
		results,
		availableIndex,
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

fiftyoneDegreesResultsNoValueReason fiftyoneDegreesResultsIpiGetNoValueReason(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception) {
	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	return resultsIpiGetNoValueReason(
		results,
		PropertiesGetAvailableIndex(
			dataSet->b.b.available,
			requiredPropertyIndex),
		exception);
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
	int availableIndex,
	StringBuilder * const builder,
	const char* separator,
	fiftyoneDegreesException* exception) {
	const WeightedItem *weightedItem;
	Item propertyItem;
	Property *property;
	const DataSetIpi *dataSet = (DataSetIpi *)results->b.dataSet;

	const int propertyIndex = PropertiesGetPropertyIndexFromAvailableIndex(
		dataSet->b.b.available,
		availableIndex);

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
			if (availableIndex >= 0) {
				weightedItem = resultsIpiGetValues(
					results,
					availableIndex,
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
	const int availableIndex = PropertiesGetAvailableIndexFromName(
		dataSet->b.b.available,
		propertyName);

	if (availableIndex >= 0) {
		fiftyoneDegreesResultsIpiGetValuesStringInternal(
			results,
			availableIndex,
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

	const DataSetIpi * const dataSet = (DataSetIpi*)results->b.dataSet;
	StringBuilder builder = { buffer, bufferLength };
	StringBuilderInit(&builder);

	fiftyoneDegreesResultsIpiGetValuesStringInternal(
		results,
		PropertiesGetAvailableIndex(
			dataSet->b.b.available,
			requiredPropertyIndex),
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

/**
 * Gets the data file property at `propertyIndex` into `item` and returns it,
 * or returns NULL, releasing a property read with an exception set.
 */
static const Property* ipiMetaDataProperty(
	DataSetIpi * const dataSet,
	const uint32_t propertyIndex,
	Item * const item,
	Exception * const exception) {
	DataReset(&item->data);
	const Property * const property = PropertyGet(
		dataSet->properties,
		propertyIndex,
		item,
		exception);
	if (property != NULL && EXCEPTION_FAILED) {
		COLLECTION_RELEASE(dataSet->properties, item);
		return NULL;
	}
	return property;
}

uint32_t fiftyoneDegreesIpiGetPropertyCount(
	fiftyoneDegreesDataSetIpi *dataSet) {
	return CollectionGetCount(dataSet->properties);
}

int fiftyoneDegreesIpiGetPropertyIndex(
	fiftyoneDegreesDataSetIpi *dataSet,
	const char *propertyName,
	fiftyoneDegreesException *exception) {
	return PropertyGetIndexByName(
		dataSet->properties,
		dataSet->strings,
		propertyName,
		exception);
}

bool fiftyoneDegreesIpiGetProperty(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint8_t *valueType,
	uint8_t *componentId,
	uint8_t *displayOrder,
	bool *isMandatory,
	bool *isList,
	bool *isObsolete,
	bool *show,
	bool *showValues,
	uint32_t *mapCount,
	bool *isAvailable,
	int *requiredPropertyIndex,
	fiftyoneDegreesException *exception) {
	Item item;
	const Property * const held = ipiMetaDataProperty(
		dataSet,
		propertyIndex,
		&item,
		exception);
	if (held == NULL) {
		return false;
	}
	// A copy, so the name is read with no other item held.
	const Property property = *held;
	COLLECTION_RELEASE(dataSet->properties, &item);
	if (componentId != NULL) {
		if (property.componentIndex >= dataSet->componentsList.count) {
			EXCEPTION_SET(COLLECTION_INDEX_OUT_OF_RANGE);
			return false;
		}
		*componentId = ((Component*)dataSet->componentsList.items[
			property.componentIndex].data.ptr)->componentId;
	}
	if (valueType != NULL) {
		*valueType = property.valueType;
	}
	if (displayOrder != NULL) {
		*displayOrder = property.displayOrder;
	}
	if (isMandatory != NULL) {
		*isMandatory = property.isMandatory != 0;
	}
	if (isList != NULL) {
		*isList = property.isList != 0;
	}
	if (isObsolete != NULL) {
		*isObsolete = property.isObsolete != 0;
	}
	if (show != NULL) {
		*show = property.show != 0;
	}
	if (showValues != NULL) {
		*showValues = property.showValues != 0;
	}
	if (mapCount != NULL) {
		*mapCount = property.mapCount;
	}
	if (isAvailable != NULL || requiredPropertyIndex != NULL) {
		// The available properties are found by name, matched ignoring case.
		DataReset(&item.data);
		const String * const name = PropertyGetName(
			dataSet->strings,
			&property,
			&item,
			exception);
		if (EXCEPTION_FAILED) {
			if (name != NULL) {
				COLLECTION_RELEASE(dataSet->strings, &item);
			}
			return false;
		}
		const char * const value = STRING(name);
		if (isAvailable != NULL) {
			*isAvailable = value != NULL &&
				PropertiesGetPropertyIndexFromName(
					dataSet->b.b.available,
					value) >= 0;
		}
		if (requiredPropertyIndex != NULL) {
			*requiredPropertyIndex = value == NULL
				? -1
				: PropertiesGetRequiredPropertyIndexFromName(
					dataSet->b.b.available,
					value);
		}
		if (name != NULL) {
			COLLECTION_RELEASE(dataSet->strings, &item);
		}
	}
	return true;
}

bool fiftyoneDegreesIpiGetPropertyValueIndexes(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint32_t *firstValueIndex,
	uint32_t *valueCount,
	long *defaultValueIndex,
	fiftyoneDegreesException *exception) {
	Item item;
	const Property * const held = ipiMetaDataProperty(
		dataSet,
		propertyIndex,
		&item,
		exception);
	if (held == NULL) {
		return false;
	}
	// A copy, so the default value is found with no other item held.
	const Property property = *held;
	COLLECTION_RELEASE(dataSet->properties, &item);
	if (firstValueIndex != NULL) {
		*firstValueIndex = property.firstValueIndex;
	}
	if (valueCount != NULL) {
		*valueCount = PropertyGetValueCount(dataSet->values, &property);
	}
	if (defaultValueIndex != NULL) {
		*defaultValueIndex = ValueGetDefaultIndex(
			dataSet->values,
			dataSet->strings,
			dataSet->propertyTypes,
			&property,
			exception);
	}
	return EXCEPTION_OKAY;
}

char* fiftyoneDegreesIpiGetPropertyText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	fiftyoneDegreesPropertyText text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception) {
	Item item;
	const Property * const property = ipiMetaDataProperty(
		dataSet,
		propertyIndex,
		&item,
		exception);
	if (property == NULL) {
		return NULL;
	}
	char * const result = PropertyGetText(
		dataSet->values,
		dataSet->strings,
		dataSet->propertyTypes,
		property,
		text,
		buffer,
		bufferLength,
		exception);
	COLLECTION_RELEASE(dataSet->properties, &item);
	return result;
}

char* fiftyoneDegreesIpiGetPropertyMapText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint32_t mapIndex,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception) {
	Item item;
	const Property * const property = ipiMetaDataProperty(
		dataSet,
		propertyIndex,
		&item,
		exception);
	if (property == NULL) {
		return NULL;
	}
	char * const result = PropertyGetMapText(
		dataSet->strings,
		dataSet->maps,
		property,
		mapIndex,
		buffer,
		bufferLength,
		exception);
	COLLECTION_RELEASE(dataSet->properties, &item);
	return result;
}

/**
 * Gets the value at `valueIndex` into `item` and returns it, or returns NULL,
 * releasing a value read with an exception set.
 */
static const Value* ipiMetaDataValue(
	DataSetIpi * const dataSet,
	const uint32_t valueIndex,
	Item * const item,
	Exception * const exception) {
	DataReset(&item->data);
	const Value * const value = ValueGet(
		dataSet->values,
		valueIndex,
		item,
		exception);
	if (value != NULL && EXCEPTION_FAILED) {
		COLLECTION_RELEASE(dataSet->values, item);
		return NULL;
	}
	return value;
}

/**
 * An empty string, which is the text the metadata gives as the description
 * of every IP Intelligence value. The size of a string counts its
 * terminating null.
 */
static const StoredBinaryValue ipiNoText = { { 1, '\0' } };

/**
 * Writes the empty text, in `buffer` when it has room and otherwise in
 * memory the caller frees.
 */
static char* ipiEmptyText(
	char * const buffer,
	const size_t bufferLength,
	Exception * const exception) {
	return StoredBinaryValueToText(
		&ipiNoText,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
		0,
		buffer,
		bufferLength,
		exception);
}

bool fiftyoneDegreesIpiGetValue(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	uint32_t *propertyIndex,
	void *state,
	fiftyoneDegreesValueTextMethod method,
	fiftyoneDegreesException *exception) {
	Item item;
	const Value * const held = ipiMetaDataValue(
		dataSet,
		valueIndex,
		&item,
		exception);
	if (held == NULL) {
		return false;
	}
	// A copy, so the value is released before its strings are read and only
	// one item is held at a time.
	const Value value = *held;
	COLLECTION_RELEASE(dataSet->values, &item);
	if (propertyIndex != NULL) {
		*propertyIndex = (uint32_t)value.propertyIndex;
	}
	if (method == NULL) {
		return true;
	}
	for (int text = FIFTYONE_DEGREES_VALUE_TEXT_NAME;
		text <= FIFTYONE_DEGREES_VALUE_TEXT_URL;
		text++) {
		// The metadata gives no description for an IP Intelligence value.
		if (text == FIFTYONE_DEGREES_VALUE_TEXT_DESCRIPTION) {
			method(state, (ValueText)text, "");
			continue;
		}
		char buffer[REASONABLE_WKT_STRING_LENGTH];
		char * const written = ValueGetMetaDataText(
			dataSet->strings,
			dataSet->propertyTypes,
			&value,
			(ValueText)text,
			DefaultWktDecimalPlaces,
			buffer,
			sizeof(buffer),
			exception);
		if (written == NULL) {
			return false;
		}
		method(state, (ValueText)text, written);
		if (written != buffer) {
			Free(written);
		}
	}
	return true;
}

long fiftyoneDegreesIpiGetValueIndex(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	const char *valueName,
	fiftyoneDegreesException *exception) {
	Item item;
	const Property * const held = ipiMetaDataProperty(
		dataSet,
		propertyIndex,
		&item,
		exception);
	if (held == NULL) {
		return -1;
	}
	// A copy, so only one item is held at a time during the search.
	const Property property = *held;
	COLLECTION_RELEASE(dataSet->properties, &item);
	const PropertyValueType storedValueType = PropertyGetStoredType(
		dataSet->propertyTypes,
		&property,
		exception);
	if (EXCEPTION_FAILED || (int32_t)property.firstValueIndex == -1) {
		return -1;
	}
	const long index = ValueGetIndexByNameAndType(
		dataSet->values,
		dataSet->strings,
		&property,
		storedValueType,
		valueName,
		exception);
	if (index < 0 || EXCEPTION_FAILED) {
		return -1;
	}
	// The search answers as found when it reads nothing for a value, so the
	// value is only found when it can be read.
	const Value * const value = ipiMetaDataValue(
		dataSet,
		(uint32_t)index,
		&item,
		exception);
	if (value == NULL) {
		return -1;
	}
	COLLECTION_RELEASE(dataSet->values, &item);
	return index;
}

char* fiftyoneDegreesIpiGetValueText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	fiftyoneDegreesValueText text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception) {
	Item item;
	const Value * const held = ipiMetaDataValue(
		dataSet,
		valueIndex,
		&item,
		exception);
	if (held == NULL) {
		return NULL;
	}
	// A copy, so the value is released before its strings are read and only
	// one item is held at a time.
	const Value value = *held;
	COLLECTION_RELEASE(dataSet->values, &item);
	// The metadata gives no description for an IP Intelligence value, and
	// writes a number in a value's name to DefaultWktDecimalPlaces places.
	if (text == FIFTYONE_DEGREES_VALUE_TEXT_DESCRIPTION) {
		return ipiEmptyText(buffer, bufferLength, exception);
	}
	return ValueGetMetaDataText(
		dataSet->strings,
		dataSet->propertyTypes,
		&value,
		text,
		DefaultWktDecimalPlaces,
		buffer,
		bufferLength,
		exception);
}
