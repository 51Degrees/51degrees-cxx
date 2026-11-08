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
 
#ifndef FIFTYONE_DEGREES_PROPERTY_META_DATA_BUILDER_HASH_HPP
#define FIFTYONE_DEGREES_PROPERTY_META_DATA_BUILDER_HASH_HPP

#include <vector>
#include "../../common/PropertyMetaData.hpp"
#include "../../common/EntityMetaDataBuilder.hpp"
#include "hash.h"

using namespace FiftyoneDegrees::Common;

namespace FiftyoneDegrees {
	namespace DeviceDetection {
		namespace Hash {
			/**
			 * Meta data builder class contains static helper methods used when
			 * building property meta data instances from a Hash data set.
			 */
			class PropertyMetaDataBuilderHash : EntityMetaDataBuilder {
			public:
				/**
				 * Build a new instance of PropertyMetaData from the property
				 * at the index provided. The instance returned does not hold
				 * a reference to the data set or property, and contains a
				 * copy of all values.
				 * @param dataSet pointer to the data set containing the
				 * property
				 * @param propertyIndex index of the property in the data file
				 * @return the new instance, or nullptr when the data set gives
				 * no property at the index
				 */
				static PropertyMetaData* build(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t propertyIndex);
			private:
				/**
				 * Get a copy of a text of the property, as
				 * fiftyoneDegreesHashGetPropertyText gives it.
				 * @param dataSet pointer to the data set containing the
				 * property
				 * @param propertyIndex index of the property
				 * @param text which text to get
				 * @return copy of the text
				 */
				static string getText(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t propertyIndex,
					fiftyoneDegreesPropertyText text);

				/**
				 * Get a copy of the type name for a property.
				 * @param valueType the type of the property's values
				 * @param isList true when the property can have more than
				 * one value
				 * @return copy of the type string
				 */
				static string getPropertyType(uint8_t valueType, bool isList);

				/**
				 * Get the names of the maps the property is in, leaving out
				 * a map the data set gives no name for.
				 * @param dataSet pointer to the data set containing the
				 * property
				 * @param propertyIndex index of the property
				 * @param mapCount the number of maps the property is in
				 * @return a new vector containing copies of the map names
				 */
				static vector<string> getPropertyMap(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t propertyIndex,
					uint32_t mapCount);

				/**
				 * Get the indexes of the properties whose values are evidence
				 * for the property with the name provided, when the data set
				 * was created with it.
				 * @param dataSet pointer to the data set containing the
				 * property
				 * @param name of the property
				 * @return the indexes of the evidence properties
				 */
				static vector<uint32_t> getEvidenceProperties(
					fiftyoneDegreesDataSetHash *dataSet,
					const string &name);
			};
		}
	}
}

#endif