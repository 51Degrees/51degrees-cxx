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

#ifndef FIFTYONE_DEGREES_VALUE_META_DATA_BUILDER_HASH_HPP
#define FIFTYONE_DEGREES_VALUE_META_DATA_BUILDER_HASH_HPP

#include <vector>
#include "../../common/ValueMetaData.hpp"
#include "../../common/EntityMetaDataBuilder.hpp"
#include "hash.h"

using namespace FiftyoneDegrees::Common;

namespace FiftyoneDegrees {
	namespace DeviceDetection {
		namespace Hash {
			/**
			 * Meta data builder class contains static helper methods used when
			 * building value meta data instances from a Hash data set.
			 */
			class ValueMetaDataBuilderHash: EntityMetaDataBuilder {
			public:
				/**
				 * Build a new instance of ValueMetaData from the value at the
				 * index provided. The instance returned does not hold a
				 * reference to the data set or value, and contains a copy of
				 * all meta data.
				 * @param dataSet pointer to the data set containing the value
				 * @param valueIndex index of the value in the values
				 * collection
				 * @return the new instance, or nullptr when the data set gives
				 * no value at the index or no property for the value
				 */
				static ValueMetaData* build(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t valueIndex);

				/**
				 * Build a new instance of ValueMetaData from the value at the
				 * index provided, which is a value of the property named, so
				 * the property's name is not read again.
				 * @param dataSet pointer to the data set containing the value
				 * @param valueIndex index of the value in the values
				 * collection
				 * @param propertyName name of the value's property
				 * @return the new instance, or nullptr when the data set gives
				 * no value at the index
				 */
				static ValueMetaData* build(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t valueIndex,
					const std::string &propertyName);

				/**
				 * Build a new instance of ValueMetaData from the value at the
				 * index provided, taking its property's name from the names
				 * given rather than reading it again.
				 * @param dataSet pointer to the data set containing the value
				 * @param valueIndex index of the value in the values
				 * collection
				 * @param propertyNames the name of each property, by the
				 * property's index
				 * @return the new instance, or nullptr when the data set gives
				 * no value at the index
				 */
				static ValueMetaData* build(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t valueIndex,
					const std::vector<std::string> &propertyNames);
				/**
				 * Build a new instance of ValueMetaData from the underlying
				 * value provided.
				 * @deprecated Use build(dataSet, valueIndex), which reads the
				 * value once in C. Kept for one release, as a public name.
				 * @param dataSet pointer to the data set containing the value
				 * @param value pointer to the underlying value to create the
				 * meta data from
				 * @return the new instance, or nullptr when the data set gives
				 * no property for the value
				 */
				static ValueMetaData* build(
					fiftyoneDegreesDataSetHash *dataSet,
					const fiftyoneDegreesValue *value);
			private:
				/**
				 * Build a new instance of ValueMetaData from the value at the
				 * index provided, reading the name of its property when
				 * neither its name nor the names of every property are given.
				 */
				static ValueMetaData* buildValue(
					fiftyoneDegreesDataSetHash *dataSet,
					uint32_t valueIndex,
					const std::string *propertyName,
					const std::vector<std::string> *propertyNames);
			};
		}
	}
}

#endif