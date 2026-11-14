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

#ifndef FIFTYONE_DEGREES_ENTITY_META_DATA_BUILDER_HPP
#define FIFTYONE_DEGREES_ENTITY_META_DATA_BUILDER_HPP

#include <string>
#include "Exceptions.hpp"
#include "collection.h"
#include "constants.h"
#include "memory.h"
#include "storedBinaryValue.h"
#include "string.h"
#include "string_pp.hpp"
#include "value.h"


using namespace FiftyoneDegrees;

namespace FiftyoneDegrees {
	namespace Common {
	FIFTYONE_DEGREES_COMMON_CXX_BEGIN
		/**
		 * Meta data builder class contains static helper methods used when
		 * building meta data instances.
		 */
		class EntityMetaDataBuilder {
		protected:

			/**
			 * @name Static Helpers
			 * @{
			 */

			/**
			 * Get a copy of a string from the strings collection at the offset
			 * provided.
			 * @param stringsCollection pointer to the collection to copy the
			 * string from
			 * @param offset offset in the strings collection of the string to
			 * copy
			 * @return string copy of the string at the offset provided
			 */
			static string getString(
				fiftyoneDegreesCollection *stringsCollection,
				uint32_t offset) {
				FIFTYONE_DEGREES_EXCEPTION_CREATE;
				string result;
				fiftyoneDegreesCollectionItem item;
				fiftyoneDegreesDataReset(&item.data);
				const fiftyoneDegreesStoredBinaryValue * const value =
					fiftyoneDegreesStoredBinaryValueGet(
						stringsCollection,
						offset,
						FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
						&item,
						exception);
				FIFTYONE_DEGREES_EXCEPTION_THROW;
				if (value != nullptr) {
					char buffer[FIFTYONE_DEGREES_REASONABLE_WKT_STRING_LENGTH];
					// A string has no decimal places.
					char * const text = fiftyoneDegreesStoredBinaryValueToText(
						value,
						FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
						0,
						buffer,
						sizeof(buffer),
						exception);
					FIFTYONE_DEGREES_COLLECTION_RELEASE(
						stringsCollection,
						&item);
					result = takeText(text, buffer, exception);
				}
				return result;
			}

			/**
			 * Get a copy of the text a C function wrote into the buffer
			 * provided or into memory it allocated, freeing that memory.
			 * @param text the text the C function returned, or nullptr
			 * @param buffer the buffer the C function was given
			 * @param exception the exception the C function was given, thrown
			 * when it is set
			 * @return copy of the text, or an empty string when there is none
			 */
			static string takeText(
				char *text,
				const char *buffer,
				fiftyoneDegreesException *exception) {
				string result;
				if (text != nullptr) {
					result = text;
					if (text != buffer) {
						fiftyoneDegreesFree(text);
					}
				}
				FIFTYONE_DEGREES_EXCEPTION_THROW;
				return result;
			}

			/**
			 * The texts of a value that addValueText copies.
			 */
			struct ValueTexts {
				string name;
				string description;
				string url;
				/** True when a text could not be copied. */
				bool failed = false;
			};

			/**
			 * Copies a text of a value into the ValueTexts that `state`
			 * points to. A failure to copy is recorded rather than thrown
			 * through the C layer that calls this.
			 * @param state pointer to the ValueTexts
			 * @param text which text of the value `value` is
			 * @param value the text to copy
			 */
			static void addValueText(
				void *state,
				fiftyoneDegreesValueText text,
				const char *value) {
				ValueTexts * const texts = (ValueTexts*)state;
				try {
					switch (text) {
					case FIFTYONE_DEGREES_VALUE_TEXT_NAME:
						texts->name = value;
						break;
					case FIFTYONE_DEGREES_VALUE_TEXT_DESCRIPTION:
						texts->description = value;
						break;
					case FIFTYONE_DEGREES_VALUE_TEXT_URL:
						texts->url = value;
						break;
					}
				}
				catch (...) {
					texts->failed = true;
				}
			}

			/**
			 * @}
			 */
		};
	FIFTYONE_DEGREES_COMMON_CXX_END
	}
}

#endif