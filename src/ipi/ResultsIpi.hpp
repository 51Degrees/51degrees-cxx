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

#ifndef FIFTYONE_DEGREES_RESULTS_IPI_HPP
#define FIFTYONE_DEGREES_RESULTS_IPI_HPP

#include <sstream>
#include <vector>
#include "../common/ResultsBase.hpp"
#include "WeightedValue.hpp"
#include "../common/IpAddress.hpp"
#include "ipi.h"
#include <functional>


class EngineIpIntelligenceTests;
class IpiFirstReadJsonTests;
class IpiFirstReadRuleTests;

namespace FiftyoneDegrees {
	namespace IpIntelligence {
		using std::shared_ptr;
		using std::string;
		using std::vector;
		using FiftyoneDegrees::Common::ResultsBase;

		/**
		 * Encapsulates the results of an IP Intelligence engine's
		 * processing. The class is constructed using an instance of a C
		 * #fiftyoneDegreesResultsIpi structure which are then
		 * referenced to return associated values and metrics.
		 *
		 * The key used to get the value for a property can be either the
		 * name of the property, or its required property index, as
		 * ResultsBase describes.
		 *
		 * An engine created naming its properties walks the graph of each
		 * component a named property belongs to as it processes the
		 * evidence, and a walk that fails throws from the process call.
		 * Otherwise the first read of a property of a component walks the
		 * component's graph, against the data set the results hold. A walk
		 * that fails throws from that read and is not repeated, so later
		 * reads of the component's properties read as if it had no profile.
		 *
		 * A results instance is used by one thread at a time, because
		 * reading a value writes into the results and, when no properties
		 * were named, the first read of a component walks its graph.
		 * Different results instances can be used on different threads at
		 * once, and the engine that created them can be shared, when
		 * EngineBase::getIsThreadSafe is true.
		 *
		 * Results instances should only be created by an Engine.
		 *
		 * ## Usage Example
		 *
		 * ```
		 * using namespace FiftyoneDegrees::IpIntelligence;
		 * ResultsIpi *results;
		 *
		 * // Get property value as a coordinate for property "AverageLocation"
		 * Value<fiftyoneDegreesCoordinate> coordinate =
		 *	getValueAsCoordinate("AverageLocation");
		 *
		 * // Get the network id for the IP range
		 * string networkId = results->getNetworkId();
		 *
		 * // Delete the results
		 * delete results;
		 * ```
		 */
		class ResultsIpi : public ResultsBase {
			friend class ::EngineIpIntelligenceTests;
			friend class ::IpiFirstReadJsonTests;
			friend class ::IpiFirstReadRuleTests;
		public:
			/**
			 * @name Constructors and Destructors
			 * @{
			 */

			 /**
			  * @copydoc Common::ResultsBase::ResultsBase
			  */
			ResultsIpi(
				fiftyoneDegreesResultsIpi *results,
				shared_ptr<fiftyoneDegreesResourceManager> manager);

			/**
			 * Release the reference to the underlying results
			 * and associated data set.
			 */
			~ResultsIpi() override;

			/**
			 * @}
			 * @name Value Getters
			 * @{
			 */

			/**
			 * Get a vector with all weighted boolean representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted boolean values for the property
			 */
			Common::Value<vector<WeightedValue<bool>>>
				getValuesAsWeightedBoolList(const char *propertyName);

			/**
			 * Get a vector with all weighted boolean representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted boolean values for the property
			 */
			Common::Value<vector<WeightedValue<bool>>>
				getValuesAsWeightedBoolList(const string *propertyName);
			
			/**
			 * Get a vector with all weighted boolean representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted boolean values for the property
			 */
			Common::Value<vector<WeightedValue<bool>>>
				getValuesAsWeightedBoolList(const string &propertyName);

			/**
			 * Get a vector with all weighted boolean representations of the 
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted boolean values for the property
			 */
			Common::Value<vector<WeightedValue<bool>>>
				getValuesAsWeightedBoolList(int requiredPropertyIndex);
			
			/**
			 * Get a vector with all weighted string representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedStringList(const char *propertyName);

			/**
			 * Get a vector with all weighted string representations of the
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<vector<WeightedValue<std::vector<uint8_t>>>>
				getValuesAsWeightedUTF8StringList(int requiredPropertyIndex);

			/**
			 * Get a vector with all weighted byte vector representations of the
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted byte vector values for the property
			 */
			Common::Value<vector<WeightedValue<std::vector<uint8_t>>>>
				getValuesAsWeightedUTF8StringList(const char *propertyName);

			/**
			 * Get a vector with all weighted byte vector representations of the
			 * values associated with the required property name. If the name
			* is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted byte vector values for the property
			 */
			Common::Value<vector<WeightedValue<std::vector<uint8_t>>>>
				getValuesAsWeightedUTF8StringList(const string &propertyName);

			/**
			 * Get a single string representations of the
			 * value associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<std::vector<uint8_t>>
				getValueAsUTF8String(int requiredPropertyIndex);

			/**
			 * Get a single string representations of the
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted byte vector values for the property
			 */
			Common::Value<std::vector<uint8_t>>
				getValueAsUTF8String(const char *propertyName);

			/**
			 * Get a single string representations of the
			 * values associated with the required property name. If the name
			* is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted byte vector values for the property
			 */
			Common::Value<std::vector<uint8_t>>
				getValueAsUTF8String(const string &propertyName);

			/**
			 * Get a vector with all weighted string representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedStringList(const string *propertyName);
			
			/**
			 * Get a vector with all weighted string representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedStringList(const string &propertyName);

			/**
			 * Get a vector with all weighted string representations of the 
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted string values for the property
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedStringList(int requiredPropertyIndex);

			/**
			 * Get a vector with all weighted string representations of the
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @param decimalPlaces precision (places after decimal dot)
			 * @return a vector of weighted string values for the property,
			 * each written from the type it is stored as, so that a geometry
			 * gives its WKT
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedWKTStringList(
					const char *propertyName, ::byte decimalPlaces);

			/**
			 * Get a vector with all weighted string representations of the
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @param decimalPlaces precision (places after decimal dot)
			 * @return a vector of weighted string values for the property,
			 * each written from the type it is stored as, so that a geometry
			 * gives its WKT
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedWKTStringList(
					const string *propertyName, ::byte decimalPlaces);

			/**
			 * Get a vector with all weighted string representations of the
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @param decimalPlaces precision (places after decimal dot)
			 * @return a vector of weighted string values for the property,
			 * each written from the type it is stored as, so that a geometry
			 * gives its WKT
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedWKTStringList(
					const string &propertyName, ::byte decimalPlaces);

			/**
			 * Get a vector with all weighted string representations of the
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @param decimalPlaces precision (places after decimal dot)
			 * @return a vector of weighted string values for the property,
			 * each written from the type it is stored as, so that a geometry
			 * gives its WKT
			 */
			Common::Value<vector<WeightedValue<string>>>
				getValuesAsWeightedWKTStringList(
				int requiredPropertyIndex, ::byte decimalPlaces);
			
			/**
			 * Get a vector with all weighted integer representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted integer values for the property
			 */
			Common::Value<vector<WeightedValue<int>>>
				getValuesAsWeightedIntegerList(const char *propertyName);

			/**
			 * Get a vector with all weighted integer representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted integer values for the property
			 */
			Common::Value<vector<WeightedValue<int>>>
				getValuesAsWeightedIntegerList(const string *propertyName);
			
			/**
			 * Get a vector with all weighted integer representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted integer values for the property
			 */
			Common::Value<vector<WeightedValue<int>>>
				getValuesAsWeightedIntegerList(const string &propertyName);

			/**
			 * Get a vector with all weighted integer representations of the 
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted integer values for the property
			 */
			Common::Value<vector<WeightedValue<int>>>
				getValuesAsWeightedIntegerList(int requiredPropertyIndex);

			/**
			 * Get a vector with all weighted double representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * A value stored as an azimuth or a declination is rounded
			 * to the decimal places of its text, which is 3 by default.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted double values for the property
			 */
			Common::Value<vector<WeightedValue<double>>>
				getValuesAsWeightedDoubleList(const char *propertyName);

			/**
			 * Get a vector with all weighted double representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * A value stored as an azimuth or a declination is rounded
			 * to the decimal places of its text, which is 3 by default.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted double values for the property
			 */
			Common::Value<vector<WeightedValue<double>>>
				getValuesAsWeightedDoubleList(const string *propertyName);
			
			/**
			 * Get a vector with all weighted double representations of the 
			 * values associated with the required property name. If the name
			 * is not valid an empty vector is returned.
			 * A value stored as an azimuth or a declination is rounded
			 * to the decimal places of its text, which is 3 by default.
			 * @param propertyName pointer to a string containing the property
			 * name
			 * @return a vector of weighted double values for the property
			 */
			Common::Value<vector<WeightedValue<double>>>
				getValuesAsWeightedDoubleList(const string &propertyName);

			/**
			 * Get a vector with all weighted double representations of the 
			 * values associated with the required property index. If the index
			 * is not valid an empty vector is returned.
			 * A value stored as an azimuth or a declination is rounded
			 * to the decimal places of its text, which is 3 by default.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return a vector of weighted double values for the property
			 */
			Common::Value<vector<WeightedValue<double>>>
			getValuesAsWeightedDoubleList(int requiredPropertyIndex);

			/**
			 * Get an IpAddress instance representation of the value associated 
			 * with the required property name. If the property name is not valid
			 * then hasValue returns false with NoValueReason and its message.
			 * @param propertyName string containing the property name
			 * @return an IpAddress representation of the value for the property
			 */
			Common::Value<IpIntelligence::IpAddress> getValueAsIpAddress(
				const char *propertyName);

			/**
			 * Get an IpAddress instance representation of the value associated 
			 * with the required property name. If the property name is not valid
			 * then hasValue returns false with NoValueReason and its message.
			 * @param propertyName string containing the property name
			 * @return an IpAddress representation of the value for the property
			 */
			Common::Value<IpIntelligence::IpAddress> getValueAsIpAddress(
				const string &propertyName);

			/**
			 * Get an IpAddress instance representation of the value associated 
			 * with the required property name. If the property name is not valid
			 * then hasValue returns false with NoValueReason and its message.
			 * @param propertyName string containing the property name
			 * @return an IpAddress representation of the value for the property
			 */
			Common::Value<IpIntelligence::IpAddress> getValueAsIpAddress(
				const string *propertyName);
			
			/**
			 * Get an IpAddress instance representation of the value associated 
			 * with the required property index. If the index is not valid
			 * then hasValue returns false with NoValueReason and its message.
			 * @param requiredPropertyIndex index of the property in the names the
			 * engine was created with, or when it named none, in the
			 * properties of the first data file it loaded
			 * @return an IpAddress representation of the value for the property
			 */
			Common::Value<IpIntelligence::IpAddress> getValueAsIpAddress(
				int requiredPropertyIndex);

			/**
			 * Get the IP address which the lookup actually resolved from the
			 * input. When the results were built from evidence this is the
			 * value of the header that the native selection order picked, so
			 * a caller can echo the client IP consistently with the location
			 * properties beside it without mirroring that order itself. When
			 * the results were built from an IP address or an IP address
			 * string it is that address. If no result was produced, for
			 * example because the evidence held no usable IP address, then
			 * hasValue returns false with NoValueReason and its message.
			 * @return the target IP address the lookup resolved
			 */
			Common::Value<IpIntelligence::IpAddress> getTargetIpAddress();

		protected:
			void getValuesInternal(
				int requiredPropertyIndex,
				vector<string> &values) override;

			bool hasValuesInternal(int requiredPropertyIndex) override;

			const char* getNoValueMessageInternal(
				fiftyoneDegreesResultsNoValueReason reason) override;

			fiftyoneDegreesResultsNoValueReason getNoValueReasonInternal(
				int requiredPropertyIndex) override;

		private:
			/**
			 * Utility function to check the property value type
			 * Consumers of this function should always check for
			 * exception status before using the returned value.
			 * @param requiredPropertyIndex index in the required
			 * properties list
			 * @param exception object which is used when an exception
			 * occurs
			 * @return the value type of the property
			 */
			fiftyoneDegreesPropertyValueType getPropertyValueType(
				int requiredPropertyIndex,
				fiftyoneDegreesException *exception);

			/**
			 * Iterates over available values for the caller to populate results.
			 * @param requiredPropertyIndex index in the required
			 * properties list
			 * @param onNoValue called if no values are available for property
			 * @param onValuesCount called when values count is known and positive
			 * @param onEachValue called for each value available
			 * @param onAfterValues called after all values have been iterated successfully
			 */
			void iterateWeightedValues(
				int requiredPropertyIndex,
				const std::function<void(
					fiftyoneDegreesResultsNoValueReason reason,
					const char *reasonStr)>& onNoValue,
				const std::function<void(uint32_t count)>& onValuesCount,
				const std::function<void(
					const fiftyoneDegreesStoredBinaryValue *binaryValue,
					fiftyoneDegreesPropertyValueType storedValueType,
					uint32_t rawWeighting,
					fiftyoneDegreesException *exception)>& onEachValue,
				const std::function<void()>& onAfterValues);

			/**
			 * The reads behind the public methods, as ResultsBase describes.
			 */
			Common::Value<vector<WeightedValue<bool>>>
				valuesAsWeightedBoolListAt(int requiredPropertyIndex);
			Common::Value<vector<WeightedValue<std::vector<uint8_t>>>>
				valuesAsWeightedUTF8StringListAt(int requiredPropertyIndex);
			Common::Value<std::vector<uint8_t>>
				valueAsUTF8StringAt(int requiredPropertyIndex);
			Common::Value<vector<WeightedValue<string>>>
				valuesAsWeightedStringListAt(int requiredPropertyIndex);
			Common::Value<vector<WeightedValue<string>>>
				valuesAsWeightedWKTStringListAt(
					int requiredPropertyIndex,
					::byte decimalPlaces);
			Common::Value<vector<WeightedValue<int>>>
				valuesAsWeightedIntegerListAt(int requiredPropertyIndex);
			Common::Value<vector<WeightedValue<double>>>
				valuesAsWeightedDoubleListAt(int requiredPropertyIndex);
			Common::Value<IpIntelligence::IpAddress>
				valueAsIpAddressAt(int requiredPropertyIndex);

			fiftyoneDegreesResultsIpi *results;
		};
	}
}

#endif
