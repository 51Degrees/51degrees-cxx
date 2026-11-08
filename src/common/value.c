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

#include "value.h"

#include "collectionKeyTypes.h"
#include "fiftyone.h"

MAP_TYPE(Value);
MAP_TYPE(Collection);
MAP_TYPE(CollectionItem);

typedef struct value_search_t {
	const Collection *strings;
	const char *valueName;
	PropertyValueType valueType;
	StringBuilder *tempBuilder;
} valueSearch;

/**
 * The string given for a value with no description or URL. The size of a
 * string counts its terminating null.
 */
static const String noString = { 1, '\0' };

/**
 * A value with no description or URL points at offset 0, which holds the first
 * string in the file rather than either of those. Callers rely on getting a
 * string, so they get an empty one, and can release the item as they do for
 * any other string because it holds nothing.
 */
static const String* getNoString(const Collection *strings, CollectionItem *item) {
	item->handle = NULL;
	item->collection = strings;
	return &noString;
}

static int compareValueByName(
	void *state,
	Item *item,
	CollectionKey key,
	Exception *exception) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(key);
#	endif
	int result = 0;
	Item name;
	const StoredBinaryValue *value;
	valueSearch *search = (valueSearch*)state;
	DataReset(&name.data);
	if (search->tempBuilder) {
		StringBuilderInit(search->tempBuilder);
	}
	value = ValueGetContent(
		search->strings,
		(Value*)item->data.ptr,
		search->valueType,
		&name,
		exception);
	if (value != NULL && EXCEPTION_OKAY) {
		result = StoredBinaryValueCompareWithString(
			value,
			search->valueType,
			search->valueName,
			search->tempBuilder,
			exception);
		COLLECTION_RELEASE(search->strings, &name);
	}
	return result;
}

const StoredBinaryValue* fiftyoneDegreesValueGetContent(
	const Collection *strings,
	const Value *value,
	PropertyValueType storedValueType,
	CollectionItem *item,
	Exception *exception) {

	return StoredBinaryValueGet(strings, value->nameOffset, storedValueType, item, exception);
}

const String* fiftyoneDegreesValueGetName(
	const Collection *strings,
	const Value *value,
	CollectionItem *item,
	Exception *exception) {
	return &StoredBinaryValueGet(
		strings,
		value->nameOffset,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // legacy contract
		item,
		exception)->stringValue;
}

const String* fiftyoneDegreesValueGetDescription(
	const Collection *strings,
	const Value *value,
	CollectionItem *item,
	Exception *exception) {
#ifdef FIFTYONE_DEGREES_REDUCED_FILE
	// A reduced size data file does not contain value descriptions, so this
	// method cannot be implemented.
#ifdef _MSC_VER
	UNREFERENCED_PARAMETER(strings);
	UNREFERENCED_PARAMETER(value);
	UNREFERENCED_PARAMETER(item);
#endif
	EXCEPTION_SET(NOT_IMPLEMENTED)
	return NULL;
#else
	if (value->descriptionOffset <= 0) {
		return getNoString(strings, item);
	}
	return &StoredBinaryValueGet(
		strings,
		value->descriptionOffset,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // description is string
		item,
		exception)->stringValue;
#endif
}

const String* fiftyoneDegreesValueGetUrl(
	const Collection *strings,
	const Value *value,
	CollectionItem *item,
	Exception *exception) {
	// A negative field is either no URL or a masked weight.
	if (value->urlOffsetOrWeight < 0) {
		return NULL;
	}
	if (value->urlOffsetOrWeight == 0) {
		return getNoString(strings, item);
	}
	return &StoredBinaryValueGet(
		strings,
		value->urlOffsetOrWeight,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // URL is string
		item,
		exception)->stringValue;
}

const Value* fiftyoneDegreesValueGet(
	const Collection *values,
	uint32_t valueIndex,
	CollectionItem *item,
	Exception *exception) {
	const CollectionKey valueKey = {
		valueIndex,
		CollectionKeyType_Value,
	};
	return (const Value*)values->get(
		values,
		&valueKey,
		item, 
		exception);
}

long fiftyoneDegreesValueGetIndexByName(
	Collection *values,
	Collection *strings,
	Property *property,
	const char *valueName,
	Exception *exception) {

	return fiftyoneDegreesValueGetIndexByNameAndType(
		values,
		strings,
		property,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // legacy contract
		valueName,
		exception);
}

/**
 * Searches the property's values for the one named, comparing each as
 * `storedValueType`, and returns its index with the value in `item`, or -1.
 * A value that is not a string is written for the comparison into a buffer
 * three bytes longer than the name, with as many decimal places as that
 * buffer has bytes, up to MAX_DOUBLE_DECIMAL_PLACES.
 */
static long searchByNameAndType(
	const Collection * const values,
	const Collection * const strings,
	const Property * const property,
	const PropertyValueType storedValueType,
	const char * const valueName,
	Item * const item,
	Exception * const exception) {
	valueSearch search;
	search.valueName = valueName;
	search.strings = strings;
	search.valueType = storedValueType;

	const bool isString =
		storedValueType == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	const size_t requiredSize = strlen(valueName) + 3;
	char * const buffer = isString ? NULL : Malloc(requiredSize);
	StringBuilder tempBuilder = { buffer, requiredSize };
	search.tempBuilder = isString ? NULL : &tempBuilder;

	const long index = CollectionBinarySearch(
		values,
		item,
		(CollectionIndexOrOffset){property->firstValueIndex},
		(CollectionIndexOrOffset){property->lastValueIndex},
		CollectionKeyType_Value,
		(void*)&search,
		compareValueByName,
		exception);
	if (buffer) {
		Free(buffer);
	}
	return index;
}

long fiftyoneDegreesValueGetIndexByNameAndType(
	const Collection *values,
	const Collection *strings,
	const Property *property,
	fiftyoneDegreesPropertyValueType storedValueType,
	const char *valueName,
	Exception *exception) {
	Item item;
	DataReset(&item.data);
	const long index = searchByNameAndType(
		values,
		strings,
		property,
		storedValueType,
		valueName,
		&item,
		exception);
	if (EXCEPTION_OKAY) {
		COLLECTION_RELEASE(values, &item);
	}
	return index;
}

const Value* fiftyoneDegreesValueGetByName(
	const Collection *values,
	const Collection *strings,
	const Property *property,
	const char *valueName,
	CollectionItem *item,
	Exception *exception) {

	return fiftyoneDegreesValueGetByNameAndType(
		values,
		strings,
		property,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // legacy contract
		valueName,
		item,
		exception);
}

const Value* fiftyoneDegreesValueGetByNameAndType(
	const Collection * const values,
	const Collection * const strings,
	const Property * const property,
	const fiftyoneDegreesPropertyValueType storedValueType,
	const char * const valueName,
	CollectionItem * const item,
	Exception * const exception) {
	Value *value = NULL;
	if (
		(int)property->firstValueIndex != -1 &&
		searchByNameAndType(
			values,
			strings,
			property,
			storedValueType,
			valueName,
			item,
			exception) >= 0 &&
		EXCEPTION_OKAY) {
		value = (Value*)item->data.ptr;
	}
	return value;
}

/**
 * Gives an empty text, in `buffer` when it has room and otherwise in memory
 * the caller frees.
 */
static char* emptyText(
	char * const buffer,
	const size_t bufferLength,
	Exception * const exception) {
	char * const text = buffer != NULL && bufferLength > 0
		? buffer
		: (char*)Malloc(1);
	if (text == NULL) {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
		return NULL;
	}
	text[0] = '\0';
	return text;
}

/**
 * Gets the text of the value's name, as fiftyoneDegreesValueGetText does,
 * except that a name which is read but cannot be written gives an empty text
 * only when `emptyWhenUnwritable` is true, and NULL otherwise.
 */
static char* getNameText(
	const Collection * const strings,
	const Collection * const propertyTypes,
	const Value * const value,
	const uint8_t decimalPlaces,
	const bool emptyWhenUnwritable,
	char * const buffer,
	const size_t bufferLength,
	Exception * const exception) {
	PropertyValueType storedValueType =
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	if (propertyTypes != NULL) {
		storedValueType = PropertyGetStoredTypeByIndex(
			propertyTypes,
			(uint32_t)value->propertyIndex,
			exception);
		if (EXCEPTION_FAILED) {
			return NULL;
		}
	}
	Item item;
	DataReset(&item.data);
	const StoredBinaryValue * const name = ValueGetContent(
		strings,
		value,
		storedValueType,
		&item,
		exception);
	if (name == NULL) {
		return EXCEPTION_FAILED
			? NULL
			: emptyText(buffer, bufferLength, exception);
	}
	if (EXCEPTION_FAILED) {
		COLLECTION_RELEASE(strings, &item);
		return NULL;
	}
	char * const text = StoredBinaryValueToText(
		name,
		storedValueType,
		decimalPlaces,
		buffer,
		bufferLength,
		exception);
	COLLECTION_RELEASE(strings, &item);
	// Memory that cannot be allocated is a failure, not an unwritable name.
	if (text == NULL &&
		emptyWhenUnwritable &&
		!EXCEPTION_CHECK(INSUFFICIENT_MEMORY)) {
		EXCEPTION_CLEAR;
		return emptyText(buffer, bufferLength, exception);
	}
	return text;
}

char* fiftyoneDegreesValueGetText(
	const Collection * const strings,
	const Collection * const propertyTypes,
	const Value * const value,
	const uint8_t decimalPlaces,
	char * const buffer,
	const size_t bufferLength,
	Exception * const exception) {
	return getNameText(
		strings,
		propertyTypes,
		value,
		decimalPlaces,
		true,
		buffer,
		bufferLength,
		exception);
}

char* fiftyoneDegreesValueGetDefaultText(
	const Collection * const values,
	const Collection * const strings,
	const Collection * const propertyTypes,
	const Property * const property,
	char * const buffer,
	const size_t bufferLength,
	Exception * const exception) {
	if (property->defaultValueIndex == UINT32_MAX) {
		return emptyText(buffer, bufferLength, exception);
	}
	Item item;
	DataReset(&item.data);
	const Value * const held = ValueGet(
		values,
		property->defaultValueIndex,
		&item,
		exception);
	if (held == NULL) {
		return EXCEPTION_FAILED
			? NULL
			: emptyText(buffer, bufferLength, exception);
	}
	if (EXCEPTION_FAILED) {
		COLLECTION_RELEASE(values, &item);
		return NULL;
	}
	// A copy, so the value is released before its name is read and only one
	// item is held at a time.
	const Value value = *held;
	COLLECTION_RELEASE(values, &item);
	// A default that cannot be written fails, as an empty text would find a
	// value with an empty name in fiftyoneDegreesValueGetDefaultIndex.
	return getNameText(
		strings,
		propertyTypes,
		&value,
		MAX_DOUBLE_DECIMAL_PLACES,
		false,
		buffer,
		bufferLength,
		exception);
}

long fiftyoneDegreesValueGetDefaultIndex(
	const Collection * const values,
	const Collection * const strings,
	const Collection * const propertyTypes,
	const Property * const property,
	Exception * const exception) {
	char buffer[REASONABLE_WKT_STRING_LENGTH];
	char * const text = ValueGetDefaultText(
		values,
		strings,
		propertyTypes,
		property,
		buffer,
		sizeof(buffer),
		exception);
	if (text == NULL) {
		return -1;
	}
	PropertyValueType storedValueType =
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	if (propertyTypes != NULL) {
		storedValueType = PropertyGetStoredType(
			propertyTypes,
			property,
			exception);
	}
	long index = -1;
	if (EXCEPTION_OKAY && (int)property->firstValueIndex != -1) {
		Item item;
		DataReset(&item.data);
		index = searchByNameAndType(
			values,
			strings,
			property,
			storedValueType,
			text,
			&item,
			exception);
		// The search answers as found when it reads nothing for a value, so
		// a value is only found when the item holds one.
		if (index >= 0 && EXCEPTION_OKAY && item.data.ptr != NULL) {
			COLLECTION_RELEASE(values, &item);
		}
		else {
			index = -1;
		}
	}
	if (text != buffer) {
		Free(text);
	}
	return index;
}

bool fiftyoneDegreesValueIsWeighted(
	const Value *value) {
	return FIFTYONE_DEGREES_VALUE_IS_MASKED(value);
}

uint16_t fiftyoneDegreesValueGetWeight(
	const Value *value) {
	if (FIFTYONE_DEGREES_VALUE_IS_MASKED(value)) {
		return (uint16_t)(value->urlOffsetOrWeight & 0xFFFF);
	}
	return 0;
}
