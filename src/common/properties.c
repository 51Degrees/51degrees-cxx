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

#include "properties.h"
#include "fiftyone.h"

/* Array of valid property name separators */
static char separators[] = FIFTYONE_DEGREES_PROPERTIES_SEPARATORS;

/* The longest name kept, as the size of a string includes its terminator */
#define MAX_NAME_LENGTH (UINT16_MAX - 1)

PropertiesRequired PropertiesDefault = {
	NULL, // No array of properties
	0, // No required properties
	NULL, // No string with properties
	NULL // No list
};

typedef struct properties_source_t {
	void *state; /* State for the get method. Usually a data set */
	PropertiesGetMethod getName; /* Gets a property as a string from the
								 source, setting the collection item
								 containing the string */
	EvidencePropertiesGetMethod getEvidence; /* Populates the evidence
											 properties */
} propertiesSource;

typedef void(*matchedPropertyMethod)(PropertiesAvailable*, uint32_t);

/* Called with each name in a list of names */
typedef void(*nameMethod)(void *state, const char *name, size_t length);

/* Used to count the names given and the bytes their copies need */
typedef struct names_size_t {
	uint32_t count;
	size_t bytes;
} namesSize;

/* Used to copy the names given */
typedef struct names_copy_t {
	PropertyNamed *named;
	uint32_t count;
	byte *next;
} namesCopy;

static size_t getMemorySize(uint32_t count) {
	return sizeof(PropertiesAvailable) + (sizeof(PropertyAvailable) * count);
}

static uint32_t countAvailableProperties(propertiesSource *source) {
	Item stringItem;
	uint32_t i = 0;
	DataReset(&stringItem.data);
	while (source->getName(source->state, i, &stringItem) != NULL) {
		COLLECTION_RELEASE(stringItem.collection, &stringItem);
		i++;
	}
	return i;
}

static PropertiesAvailable* initRequiredPropertiesMemory(uint32_t count) {
	uint32_t i;
	PropertiesAvailable *available = (PropertiesAvailable*)Malloc(
		getMemorySize(count));
	if (available != NULL) {
		available->count = 0;
		available->capacity = count;
		available->allComponentsRequired = false;
		available->namedCount = 0;
		available->named = NULL;
		available->items = (PropertyAvailable*)(available + 1);
		for (i = 0; i < available->capacity; i++) {
			// Initialize the evidence properties to prevent them from being
			// freed in the case that they are never allocated.
			available->items[i].evidenceProperties = NULL;
			// Initialize the delay execution.
			available->items[i].delayExecution = false;
			// Initialize the component index. The engine sets the real value
			// once it knows which component each property belongs to.
			available->items[i].componentIndex = 0;
			// Set once the list is sorted.
			available->items[i].requiredPropertyIndex = -1;
		}
	}
	return available;
}

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4100)
#endif
static void increaseRequiredPropertiesCount(
	PropertiesAvailable *available,
	uint32_t propertyIndex) {
	available->count++;
}
#ifdef _MSC_VER
#pragma warning (pop)
#endif

static bool resultsContain(
	PropertiesAvailable *available,
	uint32_t propertyIndex) {
	uint32_t i;
	for (i = 0; i < available->count; i++) {
		if (available->items[i].propertyIndex == propertyIndex) {
			return true;
		}
	}
	return false;
}

static void addRequiredProperty(
	PropertiesAvailable *available,
	uint32_t propertyIndex) {
	// A name read can fail while the properties are counted and work in this
	// pass, so the list is never filled past what was counted.
	if (available->count < available->capacity &&
		resultsContain(available, propertyIndex) == false) {
		available->items[available->count++].propertyIndex = propertyIndex;
	}
}

static int getPropertyIndex(
	propertiesSource *source,
	const char *requiredPropertyName,
	int requiredPropertyLength) {
	const String *test;
	Item stringItem;
	uint32_t i = 0;
	DataReset(&stringItem.data);
	test = source->getName(source->state, i, &stringItem);
	while (test != NULL) {
		if (test->size - 1 == requiredPropertyLength &&
			test != NULL &&
			_strnicmp(&test->value,
				requiredPropertyName,
				requiredPropertyLength) == 0) {
			COLLECTION_RELEASE(stringItem.collection, &stringItem);
			return i;
		}
		COLLECTION_RELEASE(stringItem.collection, &stringItem);
		test = source->getName(source->state, ++i, &stringItem);
	}
	return -1;
}

/**
 * Calls the method with each name in the separated list. A name is a run of
 * characters that are not separators, so a run of separators separates two
 * names and the names are numbered as the caller sees them.
 */
static void iterateNamesInString(
	const char *string,
	void *state,
	nameMethod method) {
	const char *name = NULL;
	const char *current;
	for (current = string; ; current++) {
		if (*current != '\0' && strchr(separators, *current) == NULL) {
			if (name == NULL) {
				name = current;
			}
		}
		else {
			if (name != NULL) {
				method(state, name, (size_t)(current - name));
				name = NULL;
			}
			if (*current == '\0') {
				break;
			}
		}
	}
}

static void addNameSize(void *state, const char *name, size_t length) {
	namesSize *size = (namesSize*)state;
	(void)name;
	size->count++;
	size->bytes += sizeof(uint16_t) +
		(length > MAX_NAME_LENGTH ? MAX_NAME_LENGTH : length) + 1;
}

/**
 * Copies the name into the block of names in the layout of a string from a
 * data set, so it can be returned where such a string is.
 */
static void addNameCopy(void *state, const char *name, size_t length) {
	namesCopy *copy = (namesCopy*)state;
	String *string = (String*)copy->next;
	if (length > MAX_NAME_LENGTH) {
		length = MAX_NAME_LENGTH;
	}
	string->size = (uint16_t)(length + 1);
	memcpy(&string->value, name, length);
	(&string->value)[length] = '\0';
	copy->named[copy->count].availableIndex = -1;
	copy->named[copy->count].name = string;
	copy->count++;
	copy->next += sizeof(uint16_t) + length + 1;
}

/**
 * Allocates one block for the names given and their copies. A list that
 * names nothing still has a block, as it is not the same as naming no list.
 */
static PropertyNamed* createNamed(namesSize *size, namesCopy *copy) {
	const uint32_t slots = size->count == 0 ? 1 : size->count;
	PropertyNamed *named = (PropertyNamed*)Malloc(
		sizeof(PropertyNamed) * slots + size->bytes);
	if (named != NULL) {
		copy->named = named;
		copy->count = 0;
		copy->next = (byte*)(named + slots);
	}
	return named;
}

static PropertyNamed* copyNamesFromString(
	const char *string,
	uint32_t *count) {
	namesSize size = { 0, 0 };
	namesCopy copy;
	iterateNamesInString(string, &size, addNameSize);
	PropertyNamed *named = createNamed(&size, &copy);
	if (named != NULL) {
		iterateNamesInString(string, &copy, addNameCopy);
		*count = copy.count;
	}
	return named;
}

static PropertyNamed* copyNamesFromArray(
	const char **array,
	int arrayCount,
	uint32_t *count) {
	int i;
	namesSize size = { 0, 0 };
	namesCopy copy;
	for (i = 0; i < arrayCount; i++) {
		const char *name = array[i] == NULL ? "" : array[i];
		addNameSize(&size, name, strlen(name));
	}
	PropertyNamed *named = createNamed(&size, &copy);
	if (named != NULL) {
		for (i = 0; i < arrayCount; i++) {
			const char *name = array[i] == NULL ? "" : array[i];
			addNameCopy(&copy, name, strlen(name));
		}
		*count = copy.count;
	}
	return named;
}

static PropertyNamed* copyNamesFromNamed(
	const PropertyNamed *existing,
	uint32_t existingCount,
	uint32_t *count) {
	uint32_t i;
	namesSize size = { 0, 0 };
	namesCopy copy;
	for (i = 0; i < existingCount; i++) {
		addNameSize(
			&size,
			&existing[i].name->value,
			(size_t)existing[i].name->size - 1);
	}
	PropertyNamed *named = createNamed(&size, &copy);
	if (named != NULL) {
		for (i = 0; i < existingCount; i++) {
			addNameCopy(
				&copy,
				&existing[i].name->value,
				(size_t)existing[i].name->size - 1);
		}
		*count = copy.count;
	}
	return named;
}

static void iteratePropertiesFromNamed(
	propertiesSource *source,
	PropertiesAvailable *available,
	const PropertyNamed *named,
	uint32_t namedCount,
	matchedPropertyMethod match) {
	uint32_t i;
	int propertyIndex;
	for (i = 0; i < namedCount; i++) {
		propertyIndex = getPropertyIndex(
			source,
			&named[i].name->value,
			(int)named[i].name->size - 1);
		if (propertyIndex >= 0) {
			match(available, (uint32_t)propertyIndex);
		}
	}
}

static PropertiesAvailable* initSpecificPropertiesFromNamed(
	propertiesSource *source,
	const PropertyNamed *named,
	uint32_t namedCount) {
	PropertiesAvailable counter;
	counter.count = 0;
	iteratePropertiesFromNamed(
		source,
		&counter,
		named,
		namedCount,
		increaseRequiredPropertiesCount);
	PropertiesAvailable *available = initRequiredPropertiesMemory(
		counter.count);
	if (available != NULL) {
		iteratePropertiesFromNamed(
			source,
			available,
			named,
			namedCount,
			addRequiredProperty);
	}
	return available;
}

static PropertiesAvailable* initAllProperties(
	propertiesSource *source) {
	uint32_t i;
	uint32_t count = countAvailableProperties(source);
	PropertiesAvailable *available = initRequiredPropertiesMemory(count);
	if (available != NULL) {
		for (i = 0; i < count; i++) {
			available->items[i].propertyIndex = i;
		}
		available->count = count;
	}
	return available;
}

static void setPropertyNames(
	propertiesSource* source,
	PropertiesAvailable* available) {
	uint32_t i;
	for (i = 0; i < available->count; i++) {
		DataReset(&available->items[i].name.data);
		source->getName(
			source->state,
			available->items[i].propertyIndex,
			&available->items[i].name);
	}
}

static void setEvidenceProperties(
	propertiesSource* source,
	PropertiesAvailable* available) {
	uint32_t i, count;
	for (i = 0; i < available->count; i++) {
		// Get the count before populating.
		count = source->getEvidence(
			source->state,
			&available->items[i],
			NULL);
		// Allocate an array big enough to be
		// populated.
		FIFTYONE_DEGREES_ARRAY_CREATE(
			fiftyoneDegreesEvidencePropertyIndex,
			available->items[i].evidenceProperties,
			count);
		if (available->items[i].evidenceProperties != NULL) {
			// Now populate the array and set the counjt.
			count = source->getEvidence(
				source->state,
				&available->items[i],
				available->items[i].evidenceProperties);
			available->items[i].evidenceProperties->count = count;
		}
	}
}

static int comparePropertyNamesAscending(const void *a, const void *b) {
	PropertyAvailable *ai = (PropertyAvailable*)a;
	PropertyAvailable *bi = (PropertyAvailable*)b;
	const char *as = STRING(ai->name.data.ptr); // name is string
	const char *bs = STRING(bi->name.data.ptr); // name is string
	assert(as != NULL && bs != NULL);
	return _stricmp(as, bs);
}

static void sortRequiredProperties(
	PropertiesAvailable *available) {
	qsort(
		(void*)available->items,
		available->count,
		sizeof(PropertyAvailable),
		comparePropertyNamesAscending);
}

/**
 * Sets the names and evidence of the properties, and sorts them by name.
 * Returns false when a name could not be read, leaving the list unsorted.
 */
static bool initRequiredProperties(
	propertiesSource *source,
	PropertiesAvailable *available) {
	uint32_t i;
	String *string;

	// Set the names for each of the properties.
	setPropertyNames(source, available);

	// A name that could not be read is left missing for the caller to report,
	// as the names cannot then be compared or used to find evidence.
	for (i = 0; i < available->count; i++) {
		if (available->items[i].name.data.ptr == NULL) {
			return false;
		}
	}

	// Set the evidence properties for each of the properties.
	setEvidenceProperties(source, available);

	// Sort these names in ascending order.
	sortRequiredProperties(available);

	// The property indexes are now invalid so need to be reset from the names.
	for (i = 0; i < available->count; i++) {
		string = (String*)available->items[i].name.data.ptr;
		if (string != NULL) {
			available->items[i].propertyIndex = getPropertyIndex(
				source,
				&string->value,
				string->size - 1);
		}
	}
	return true;
}

static int comparePropertyNamesAscendingSearch(const void *a, const void *b) {
	char *as = (char*)a;
	char *bs = &((String*)((PropertyAvailable*)b)->name.data.ptr)->value;
	return StringCompare(as, bs);
}

static PropertyAvailable* findAvailable(
	PropertiesAvailable *available,
	const char *propertyName) {
	if (available == NULL || propertyName == NULL) {
		return NULL;
	}
	return (PropertyAvailable*)bsearch(
		propertyName,
		available->items,
		available->count,
		sizeof(PropertyAvailable),
		comparePropertyNamesAscendingSearch);
}

/**
 * Sets the required property index of each available property, which is the
 * place of its name among the names given, and the available index of each
 * name given. Without names, each property's index is its own place.
 */
static void setRequiredPropertyIndexes(PropertiesAvailable *available) {
	uint32_t i;
	PropertyAvailable *found;
	if (available->named == NULL) {
		for (i = 0; i < available->count; i++) {
			available->items[i].requiredPropertyIndex = (int32_t)i;
		}
		return;
	}
	for (i = 0; i < available->namedCount; i++) {
		found = findAvailable(available, &available->named[i].name->value);
		if (found == NULL) {
			available->named[i].availableIndex = -1;
		}
		else {
			available->named[i].availableIndex =
				(int32_t)(found - available->items);
			if (found->requiredPropertyIndex < 0) {
				found->requiredPropertyIndex = (int32_t)i;
			}
		}
	}
}

fiftyoneDegreesPropertiesAvailable* fiftyoneDegreesPropertiesCreate(
	fiftyoneDegreesPropertiesRequired *properties,
	void *state,
	fiftyoneDegreesPropertiesGetMethod getPropertyMethod,
	fiftyoneDegreesEvidencePropertiesGetMethod getEvidencePropertiesMethod) {
	propertiesSource source;
	source.state = state;
	source.getName = getPropertyMethod;
	source.getEvidence = getEvidencePropertiesMethod;
	PropertiesAvailable *available = NULL;
	PropertyNamed *named = NULL;
	uint32_t namedCount = 0;
	bool allComponentsRequired = false;

	// Copy the names the caller gave. Existing properties give the names they
	// were created with, so a reload starts again from the caller's names.
	if (properties != NULL) {
		if (properties->existing != NULL) {
			allComponentsRequired =
				properties->existing->allComponentsRequired;
			if (properties->existing->named != NULL) {
				named = copyNamesFromNamed(
					properties->existing->named,
					properties->existing->namedCount,
					&namedCount);
				if (named == NULL) {
					return NULL;
				}
			}
		}
		else if (properties->array != NULL && properties->count > 0) {
			named = copyNamesFromArray(
				properties->array,
				properties->count,
				&namedCount);
			if (named == NULL) {
				return NULL;
			}
		}
		else if (properties->string != NULL &&
			strlen(properties->string) > 0) {
			named = copyNamesFromString(properties->string, &namedCount);
			if (named == NULL) {
				return NULL;
			}
		}
	}

	if (named != NULL) {
		// Set the required properties from the names given.
		available = initSpecificPropertiesFromNamed(
			&source,
			named,
			namedCount);
	}
	else {
		// Set all the properties as required properties.
		available = initAllProperties(&source);
	}
	if (available == NULL) {
		if (named != NULL) {
			Free(named);
		}
		return NULL;
	}
	available->allComponentsRequired = allComponentsRequired;
	available->named = named;
	available->namedCount = namedCount;

	// Set the property name strings, sort the list by name, and then record
	// where each name given is in the sorted list. A name that could not be
	// read leaves the list unsorted for the caller to report, so no place is
	// recorded from it.
	if (initRequiredProperties(&source, available)) {
		setRequiredPropertyIndexes(available);
	}

	return available;
}

int fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
	fiftyoneDegreesPropertiesAvailable *available,
	int requiredPropertyIndex) {
	return PropertiesGetPropertyIndexFromAvailableIndex(
		available,
		PropertiesGetAvailableIndex(available, requiredPropertyIndex));
}

int fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
	fiftyoneDegreesPropertiesAvailable *available,
	const char *propertyName) {
	const PropertyAvailable *found = findAvailable(available, propertyName);
	return found == NULL ? -1 : (int)found->requiredPropertyIndex;
}

int fiftyoneDegreesPropertiesGetPropertyIndexFromName(
	fiftyoneDegreesPropertiesAvailable *available,
	const char *propertyName) {
	const PropertyAvailable *found = findAvailable(available, propertyName);
	return found == NULL ? -1 : (int)found->propertyIndex;
}

fiftyoneDegreesString* fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
	fiftyoneDegreesPropertiesAvailable *available,
	int requiredPropertyIndex) {
	int availableIndex;
	if (available == NULL || requiredPropertyIndex < 0) {
		return NULL;
	}
	if (available->named != NULL) {
		if ((uint32_t)requiredPropertyIndex >= available->namedCount) {
			return NULL;
		}
		availableIndex = available->named[requiredPropertyIndex].availableIndex;
		return availableIndex >= 0 ?
			(String*)available->items[availableIndex].name.data.ptr :
			(String*)available->named[requiredPropertyIndex].name;
	}
	return (uint32_t)requiredPropertyIndex < available->count ?
		(String*)available->items[requiredPropertyIndex].name.data.ptr :
		NULL;
}

bool fiftyoneDegreesPropertiesIsSetHeaderAvailable(
	fiftyoneDegreesPropertiesAvailable* available) {
	const char* string;
	for (uint32_t i = 0; i < available->count; i++) {
		string = FIFTYONE_DEGREES_STRING(available->items[i].name.data.ptr); // name is string
		if (StringSubString(string, "SetHeader") == string) {
			return true;
		}
	}
	return false;
}

void fiftyoneDegreesPropertiesFree(
	fiftyoneDegreesPropertiesAvailable *available) {
	uint32_t i;
	if (available != NULL) {
		for (i = 0; i < available->count; i++) {
			if (available->items[i].name.data.ptr != NULL) {
				COLLECTION_RELEASE(available->items[i].name.collection,
					&available->items[i].name);
			}
			if (available->items[i].evidenceProperties != NULL) {
				Free(available->items[i].evidenceProperties);
			}
		}
		if (available->named != NULL) {
			Free(available->named);
		}
		Free(available);
	}
}

uint32_t fiftyoneDegreesPropertiesGetComponentMask(
	fiftyoneDegreesPropertiesAvailable *available,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount) {
	uint32_t mask = 0;
	int i;
	if (requiredPropertyIndexes == NULL || requiredPropertyIndexesCount < 0) {
		return FIFTYONE_DEGREES_COMPONENT_MASK_ALL;
	}
	for (i = 0; i < requiredPropertyIndexesCount; i++) {
		int index = PropertiesGetAvailableIndex(
			available,
			requiredPropertyIndexes[i]);
		if (index >= 0) {
			byte componentIndex = available->items[index].componentIndex;
			if (componentIndex < FIFTYONE_DEGREES_COMPONENT_MASK_BITS) {
				mask |= 1u << componentIndex;
			}
		}
	}
	return mask;
}
