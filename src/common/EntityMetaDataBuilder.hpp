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
			 * @}
			 */
		};
	FIFTYONE_DEGREES_COMMON_CXX_END
	}
}

#endif