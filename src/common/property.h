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

#ifndef FIFTYONE_DEGREES_PROPERTY_H_INCLUDED
#define FIFTYONE_DEGREES_PROPERTY_H_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

/**
 * @ingroup FiftyOneDegreesCommon
 * @defgroup FiftyOneDegreesProperty Property
 *
 * Property in a data set relating to a single component.
 *
 * ## Introduction
 *
 * A property is stored in a properties collection and contains the meta data
 * for a specific property in a data set.
 *
 * ## Get
 *
 * A property can be fetched from a properties collection in one of two ways:
 *
 * **By Index** : The #fiftyoneDegreesPropertyGet method return the property at
 * a specified index. This provides a way to access a property at a known
 * index, or iterate over all properties.
 *
 * **By Name** : If the index of a property is not known, then the property can
 * be fetched using the #fiftyoneDegreesPropertyGetByName method to find the
 * property in a properties collection.
 *
 * @{
 */

#include <stdint.h>
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 5105) 
#include <windows.h>
#pragma warning (default: 5105) 
#pragma warning (pop)
#endif
#include "data.h"
#include "exceptions.h"
#include "collection.h"
#include "string.h"
#include "common.h"
#include "propertyValueType.h"

/**
 * Property structure containing all the meta data relating to a property.
 */
#pragma pack(push, 1)
typedef struct property_t {
	const byte componentIndex; /**< Index of the component */
	const byte displayOrder; /**< The order the property should be displayed in 
	                             relative to other properties */
	const byte isMandatory; /**< True if the property is mandatory and must be
	                            provided */
	const byte isList; /**< True if the property is a list can return multiple 
	                       values */
	const byte showValues; /**< True if the values should be shown in GUIs */
	const byte isObsolete; /**< True if the property is obsolete and will be 
	                           removed from future data sets */
	const byte show; /**< True if the property should be shown in GUIs */
	const byte valueType; /**< The type of value the property represents */
	const uint32_t defaultValueIndex; /**< The default value index for the
	                                     property */
	const uint32_t nameOffset; /**< The offset in the strings structure to the 
	                               property name */
	const uint32_t descriptionOffset; /**< The offset in the strings structure
	                                      to the property description */
	const uint32_t categoryOffset; /**< The offset in the strings structure to
	                                   the property category */
	const uint32_t urlOffset; /**< The offset in the strings structure to the
	                              property URL */
	const uint32_t firstValueIndex; /**< Index of the first possible value */
	const uint32_t lastValueIndex; /**< Index of the last possible value */
	const uint32_t mapCount; /**< Number of maps the property is associated with */
	const uint32_t firstMapIndex; /**< The first index in the list of maps the
	                                  property is associated with */
} fiftyoneDegreesProperty;
#pragma pack(pop)

/**
 * Property structure containing stored type of a property.
 */
#pragma pack(push, 1)
typedef struct property_type_record_t {
	const uint32_t nameOffset; /**< The offset in the strings structure to the
	                               property name */
	const byte storedValueType; /**< The type of value the property is stored as */
} fiftyoneDegreesPropertyTypeRecord;
#pragma pack(pop)

/**
 * Returns the string name of the property using the item provided. The 
 * collection item must be released when the caller is finished with the
 * string.
 * @param stringsCollection collection of strings retrieved by offsets.
 * @param property structure for the name required.
 * @param item used to store the resulting string in.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a pointer to a string in the collection item data structure.
 */
EXTERNAL const fiftyoneDegreesString* fiftyoneDegreesPropertyGetName(
	const fiftyoneDegreesCollection *stringsCollection,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * Returns the type the property is stored as.
 * @param propertyTypesCollection collection of property types retrieved by offsets.
 * @param property structure for the type required.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a type the property is stored as.
 */
EXTERNAL fiftyoneDegreesPropertyValueType fiftyoneDegreesPropertyGetStoredType(
	const fiftyoneDegreesCollection *propertyTypesCollection,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesException *exception);

/**
 * Returns the type the property is stored as.
 * @param propertyTypesCollection collection of property types retrieved by offsets.
 * @param index of the property to get
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a type the property is stored as.
 */
EXTERNAL fiftyoneDegreesPropertyValueType fiftyoneDegreesPropertyGetStoredTypeByIndex(
	const fiftyoneDegreesCollection *propertyTypesCollection,
	uint32_t index,
	fiftyoneDegreesException *exception);

/**
 * Returns the string description of the property using the item provided. The
 * collection item must be released when the caller is finished with the
 * string.
 * @param stringsCollection collection of strings retrieved by offsets.
 * @param property structure for the description required.
 * @param item used to store the resulting string in.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a pointer to a string in the collection item data structure.
 */
EXTERNAL const fiftyoneDegreesString* fiftyoneDegreesPropertyGetDescription(
	const fiftyoneDegreesCollection *stringsCollection,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * Returns the string category of the property using the item provided. The
 * collection item must be released when the caller is finished with the
 * string.
 * @param stringsCollection collection of strings retrieved by offsets.
 * @param property structure for the category required.
 * @param item used to store the resulting string in.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a pointer to a string in the collection item data structure.
 */
EXTERNAL const fiftyoneDegreesString* fiftyoneDegreesPropertyGetCategory(
	const fiftyoneDegreesCollection *stringsCollection,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * Returns the string URL of the property using the item provided. The
 * collection item must be released when the caller is finished with the
 * string.
 * @param stringsCollection collection of strings retrieved by offsets.
 * @param property structure for the URL required.
 * @param item used to store the resulting string in.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a pointer to a string in the collection item data structure, or to
 * an empty string if the property has no URL.
 */
EXTERNAL const fiftyoneDegreesString* fiftyoneDegreesPropertyGetUrl(
	const fiftyoneDegreesCollection *stringsCollection,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * Gets the value type for the property at the index in the collection.
 * @param properties collection to retrieve the property type from
 * @param index of the property in the collection
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the value type as a byte for the property at the index.
 */
EXTERNAL byte fiftyoneDegreesPropertyGetValueType(
	fiftyoneDegreesCollection *properties,
	uint32_t index,
	fiftyoneDegreesException *exception);

/**
 * Gets the property at the requested index from the properties collection
 * provided.
 * @param properties to get the property from
 * @param index of the property to get
 * @param item to store the property item in
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the property requested or NULL
 */
EXTERNAL fiftyoneDegreesProperty* fiftyoneDegreesPropertyGet(
	fiftyoneDegreesCollection *properties,
	uint32_t index,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * Gets the property with the requested name from the properties collection
 * provided.
 * @param properties to get the property from
 * @param strings collection containing the names of the properties
 * @param requiredPropertyName name of the property to get
 * @param item to store the property item in
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the property requested or NULL
 */
EXTERNAL const fiftyoneDegreesProperty* fiftyoneDegreesPropertyGetByName(
	fiftyoneDegreesCollection *properties,
	fiftyoneDegreesCollection *strings,
	const char *requiredPropertyName,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception);

/**
 * The text of a property that #fiftyoneDegreesPropertyGetText gives.
 */
typedef enum e_fiftyone_degrees_property_text {
	FIFTYONE_DEGREES_PROPERTY_TEXT_NAME = 0, /**< The property's name */
	FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION = 1, /**< The property's
	                                                description, empty when
	                                                it has none */
	FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY = 2, /**< The name of the
	                                             category the property is in,
	                                             empty when it is in none */
	FIFTYONE_DEGREES_PROPERTY_TEXT_URL = 3, /**< The URL of more about the
	                                        property, empty when it has
	                                        none */
	FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE = 4 /**< The property's
	                                                 default value, as
	                                                 #fiftyoneDegreesValueGetDefaultText
	                                                 gives it */
} fiftyoneDegreesPropertyText;

/**
 * Gets the index in the properties collection of the property with the name
 * provided, which must match exactly. This finds the property that
 * #fiftyoneDegreesPropertyGetByName gets.
 * @param properties collection to find the property in
 * @param strings collection containing the names of the properties
 * @param propertyName name of the property to find
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the 0 based index of the property, or -1 when no property has the
 * name or a property or a name cannot be read
 */
EXTERNAL int fiftyoneDegreesPropertyGetIndexByName(
	fiftyoneDegreesCollection *properties,
	fiftyoneDegreesCollection *strings,
	const char *propertyName,
	fiftyoneDegreesException *exception);

/**
 * Gets a text of the property as the metadata gives it. A description,
 * category or URL the property does not have is an empty text, as is a
 * string the strings collection gives nothing for. The text is written into
 * `buffer` when it fits, and otherwise into memory the caller frees, as
 * #fiftyoneDegreesStoredBinaryValueToText describes.
 * @param values collection containing the property's values, which only
 * #FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE reads
 * @param strings collection containing the property's strings
 * @param propertyTypes collection of the type each property's values are
 * stored as, or NULL when every value is stored as a string, which only
 * #FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE reads
 * @param property the property to get the text of
 * @param text which text of the property to get
 * @param buffer memory the text is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the null terminated text, or NULL when it cannot be read or
 * written or `text` is none of #fiftyoneDegreesPropertyText
 */
EXTERNAL char* fiftyoneDegreesPropertyGetText(
	const fiftyoneDegreesCollection *values,
	const fiftyoneDegreesCollection *strings,
	const fiftyoneDegreesCollection *propertyTypes,
	const fiftyoneDegreesProperty *property,
	fiftyoneDegreesPropertyText text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the name of one of the maps the property is in, which the metadata
 * gives as the data files the property is present in, such as "Lite". The
 * name is written as #fiftyoneDegreesPropertyGetText writes a text.
 * @param strings collection containing the names of the maps
 * @param maps collection of the offsets in `strings` of the map names
 * @param property the property to get the name of a map of
 * @param mapIndex the 0 based place of the map among the property's
 * `mapCount` maps
 * @param buffer memory the name is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the null terminated name, or NULL. NULL with no exception set
 * means the strings collection gives nothing at the map's offset, and NULL
 * with the exception set means `mapIndex` is not less than the property's
 * `mapCount` or the name cannot be read or written.
 */
EXTERNAL char* fiftyoneDegreesPropertyGetMapText(
	const fiftyoneDegreesCollection *strings,
	const fiftyoneDegreesCollection *maps,
	const fiftyoneDegreesProperty *property,
	uint32_t mapIndex,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the number of values the values collection holds for the property,
 * which are at the indexes from its `firstValueIndex` to its
 * `lastValueIndex`. A property whose value indexes are not both in the
 * collection, as when both are -1, has none.
 * @param values collection containing the property's values
 * @param property the property to count the values of
 * @return the number of values
 */
EXTERNAL uint32_t fiftyoneDegreesPropertyGetValueCount(
	const fiftyoneDegreesCollection *values,
	const fiftyoneDegreesProperty *property);

/**
 * @}
 */

#endif