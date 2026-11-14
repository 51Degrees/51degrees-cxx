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

#define __STDC_FORMAT_MACROS

#include "string.h"
#include "fiftyone.h"
#include <inttypes.h>

#include "collectionKeyTypes.h"

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY


void* fiftyoneDegreesStoredBinaryValueRead(
    const CollectionFile * const file,
    const CollectionKey * const key,
    Data * const data,
    Exception * const exception) {
#   define MAX_INITIAL_BUFFER_LENGTH 8
    if (key->keyType->initialBytesCount > MAX_INITIAL_BUFFER_LENGTH) {
        EXCEPTION_SET(FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY);
        return NULL;
    }
    byte initial[MAX_INITIAL_BUFFER_LENGTH];
#   undef MAX_INITIAL_BUFFER_LENGTH

    return CollectionReadFileVariable(
        file,
        data,
        key,
        &initial,
        exception);
}

#endif

const StoredBinaryValue* fiftyoneDegreesStoredBinaryValueGet(
    const fiftyoneDegreesCollection *strings,
    uint32_t offset,
    PropertyValueType storedValueType,
    fiftyoneDegreesCollectionItem *item,
    Exception *exception) {

    const CollectionKey key = {
        offset,
        GetCollectionKeyTypeForStoredValueType(storedValueType, exception),
    };
    if (EXCEPTION_FAILED) {
        return NULL;
    }

    const fiftyoneDegreesStoredBinaryValue * const result = strings->get(
        strings,
        &key,
        item,
        exception);
    return result;
}

static double shortToDouble(const StoredBinaryValue * const value, const double maxAngle) {
    return (value->shortValue * maxAngle) / INT16_MAX;
}
static double toAzimuth(const StoredBinaryValue * const value) {
    return shortToDouble(value, 180);
}
static double toDeclination(const StoredBinaryValue * const value) {
    return shortToDouble(value, 90);
}

int fiftyoneDegreesStoredBinaryValueCompareWithString(
    const StoredBinaryValue * const value,
    const PropertyValueType storedValueType,
    const char * const target,
    StringBuilder * const tempBuilder,
    Exception * const exception) {

    if (storedValueType == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING) {
        const int cmpResult = strncmp(
            &value->stringValue.value,
            target,
            value->stringValue.size);
        return cmpResult;
    }
    EXCEPTION_CLEAR;
    const uint8_t decimalPlaces = (
        tempBuilder->length < MAX_DOUBLE_DECIMAL_PLACES
        ? (uint8_t)tempBuilder->length
        : MAX_DOUBLE_DECIMAL_PLACES);
    StringBuilderAddStringValue(
        tempBuilder,
        value,
        storedValueType,
        decimalPlaces,
        exception);
    StringBuilderComplete(tempBuilder);
    const int result = (EXCEPTION_OKAY
        ? strcmp(tempBuilder->ptr, target)
        : -1);
    return result;
}

/**
 * Bytes the text of the value is expected to need, counting its terminating
 * null, or 0 when that is not known before it is written. A string needs its
 * size, and an area normally needs at most
 * FIFTYONE_DEGREES_TEXT_PER_GEOMETRY_BYTE for each byte, though a reduced
 * area written to more than 4 places can need more.
 */
static size_t textBytesExpected(
    const StoredBinaryValue * const value,
    const PropertyValueType storedValueType) {
    switch (storedValueType) {
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING:
            return value->stringValue.size > 0
                ? (size_t)value->stringValue.size
                : (size_t)1;
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB:
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB_R:
            return (size_t)value->byteArrayValue.size *
                FIFTYONE_DEGREES_TEXT_PER_GEOMETRY_BYTE + 1;
        default:
            return 0;
    }
}

/**
 * Writes the text of the value into the `length` bytes at `text`, where
 * `length` is at least 1, and returns the bytes the whole text needs,
 * counting its terminating null, which is more than `length` when it did not
 * fit. Returns 0 when the value cannot be written.
 */
static size_t writeText(
    const StoredBinaryValue * const value,
    const PropertyValueType storedValueType,
    const uint8_t decimalPlaces,
    char * const text,
    const size_t length,
    Exception * const exception) {
    StringBuilder builder = { text, length };
    StringBuilderInit(&builder);
    StringBuilderAddStringValue(
        &builder,
        value,
        storedValueType,
        decimalPlaces,
        exception);
    StringBuilderComplete(&builder);
    if (EXCEPTION_FAILED) {
        return 0;
    }
    // The builder counts every character, written or not, and always keeps a
    // byte for the null, which Complete counts too, so the count is the bytes
    // the text needs whether or not it fitted.
    return builder.added;
}

char* fiftyoneDegreesStoredBinaryValueToText(
    const StoredBinaryValue * const value,
    const PropertyValueType storedValueType,
    const uint8_t decimalPlaces,
    char * const buffer,
    const size_t bufferLength,
    Exception * const exception) {
    if (value == NULL) {
        EXCEPTION_SET(NULL_POINTER);
        return NULL;
    }
    const size_t expected = textBytesExpected(value, storedValueType);
    size_t needed;
    if (buffer != NULL && bufferLength > 0 && expected <= bufferLength) {
        needed = writeText(
            value,
            storedValueType,
            decimalPlaces,
            buffer,
            bufferLength,
            exception);
        if (needed == 0) {
            return NULL;
        }
        if (needed <= bufferLength) {
            return buffer;
        }
    }
    else {
        needed = expected > 0 ? expected : REASONABLE_WKT_STRING_LENGTH;
    }
    // At most twice, as a write that does not fit counts the bytes it needs.
    for (int attempt = 0; attempt < 2; attempt++) {
        const size_t length = needed;
        char * const text = (char*)Malloc(length);
        if (text == NULL) {
            EXCEPTION_SET(INSUFFICIENT_MEMORY);
            return NULL;
        }
        needed = writeText(
            value,
            storedValueType,
            decimalPlaces,
            text,
            length,
            exception);
        if (needed > 0 && needed <= length) {
            return text;
        }
        Free(text);
        if (needed == 0) {
            return NULL;
        }
    }
    EXCEPTION_SET(INSUFFICIENT_CAPACITY);
    return NULL;
}

int fiftyoneDegreesStoredBinaryValueToIntOrDefault(
    const fiftyoneDegreesStoredBinaryValue * const value,
    const fiftyoneDegreesPropertyValueType storedValueType,
    const int defaultValue) {

    switch (storedValueType) {
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING: {
            return atoi(&value->stringValue.value);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER: {
            return value->intValue;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT: {
            return (int)FLOAT_TO_NATIVE(value->floatValue);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH: {
            return (int)toAzimuth(value);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION: {
            return (int)toDeclination(value);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_BYTE: {
            return value->byteValue;
        }
        default: {
            return defaultValue;
        }
    }
}

double fiftyoneDegreesStoredBinaryValueToDoubleOrDefault(
    const fiftyoneDegreesStoredBinaryValue * const value,
    const fiftyoneDegreesPropertyValueType storedValueType,
    const double defaultValue) {

    switch (storedValueType) {
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING: {
            return strtod(&value->stringValue.value, NULL);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER: {
            return value->intValue;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT: {
            return FLOAT_TO_NATIVE(value->floatValue);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH: {
            return toAzimuth(value);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION: {
            return toDeclination(value);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_BYTE: {
            return value->byteValue;
        }
        default: {
            return defaultValue;
        }
    }
}

/** Powers of ten that a double holds exactly. */
static const double roundingScales[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7,
    1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15 };

/** The largest scaled value that is rounded through a 64 bit integer. */
#define ROUNDING_LIMIT 9e15

/**
 * Rounds the value to the decimal places, a half going away from zero. The
 * scaled value is a whole number that a double holds exactly, so dividing it
 * by the scale gives the double nearest to the rounded decimal.
 */
static double roundToPlaces(const double value, const uint8_t decimalPlaces) {
    const double scale = roundingScales[
        decimalPlaces < MAX_DOUBLE_DECIMAL_PLACES ?
        decimalPlaces : MAX_DOUBLE_DECIMAL_PLACES];
    const double scaled = value * scale;
    if (scaled >= ROUNDING_LIMIT || scaled <= -ROUNDING_LIMIT) {
        // A double this large has no fraction left to round.
        return value;
    }
    return (double)(int64_t)(scaled + (scaled < 0 ? -0.5 : 0.5)) / scale;
}

double fiftyoneDegreesStoredBinaryValueToRoundedDoubleOrDefault(
    const fiftyoneDegreesStoredBinaryValue * const value,
    const fiftyoneDegreesPropertyValueType storedValueType,
    const uint8_t decimalPlaces,
    const double defaultValue) {

    switch (storedValueType) {
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH: {
            return roundToPlaces(toAzimuth(value), decimalPlaces);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION: {
            return roundToPlaces(toDeclination(value), decimalPlaces);
        }
        default: {
            return StoredBinaryValueToDoubleOrDefault(
                value,
                storedValueType,
                defaultValue);
        }
    }
}

bool fiftyoneDegreesStoredBinaryValueToBoolOrDefault(
    const fiftyoneDegreesStoredBinaryValue * const value,
    const fiftyoneDegreesPropertyValueType storedValueType,
    const bool defaultValue) {

    switch (storedValueType) {
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING: {
            if (value->stringValue.size != 5) {
                return false;
            }
            return !strncmp(&value->stringValue.value, "True", 4);
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER: {
            return value->intValue ? true : false;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT: {
            return FLOAT_TO_NATIVE(value->floatValue) ? true : false;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH: {
            return toAzimuth(value) ? true : false;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION: {
            return toDeclination(value) ? true : false;
        }
        case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_BYTE: {
            return value->byteValue;
        }
        default: {
            return defaultValue;
        }
    }
}
