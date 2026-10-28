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

#ifndef FIFTYONE_DEGREES_ENGINE_IPI_HPP
#define FIFTYONE_DEGREES_ENGINE_IPI_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include "common-cxx/ip.h"
#include "common-cxx/EngineBase.hpp"
#include "common-cxx/resource.h"
#include "common-cxx/RequiredPropertiesConfig.hpp"
#include "common-cxx/Date.hpp"
#include "EvidenceIpi.hpp"
#include "ConfigIpi.hpp"
#include "ResultsIpi.hpp"
#include "MetaDataIpi.hpp"


namespace FiftyoneDegrees {
	namespace IpIntelligence {
		using FiftyoneDegrees::Common::EngineBase;
		using FiftyoneDegrees::Common::Date;
		using FiftyoneDegrees::Common::RequiredPropertiesConfig;

		/**
		 * Encapsulates the IP Intelligence engine class which implements
		 * #EngineBase. This carries out processing using an IP 
		 * Intelligence data set.
		 *
		 * An engine is constructed with a configuration, and either a
		 * data file, or an in memory data set, then used to process
		 * evidence in order to return a set of results. It also exposes
		 * methods to refresh the data using a new data set, and get
		 * properties relating to the data set being used by the engine.
		 *
		 * ## Usage Example
		 *
		 * ```
		 * using namespace FiftyoneDegrees::Common;
		 * using namespace FiftyoneDegrees::IpIntelligence;
		 * ConfigIpi *config;
		 * string dataFilePath;
		 * void *inMemoryDataSet;
		 * long inMemoryDataSetLength;
		 * RequiredPropertiesConfig *properties;
		 * EvidenceIpi *evidence;
		 * unsigned char ipv4Address[4];
		 *
		 * // Construct the engine from a data file
		 * EngineIpi *engine = new EngineIpi(
		 *     dataFilePath,
		 *     config,
		 *     properties);
		 *
		 * // Or from a data file which has been loaded into continuous
		 * // memory
		 * EngineIpi *engine = new EngineIpi(
		 *     inMemoryDataSet,
		 *     inMemoryDataSetLength,
		 *     config,
		 *     properties);
		 *
		 * // Process some evidence
		 * ResultsIpi *results = engine->process(evidence);
		 *
		 * // Or just process a single IP address string
		 * ResultsIpi *results = engine->process("some IP address");
		 *
		 * // Or a raw IP address byte array
		 * ResultsIpi *results = engine->process(ipv4Address, 4);
		 *
		 * // Do something with the results
		 * // ...
		 *
		 * // Delete the results and the engine
		 * delete results;
		 * delete engine;
		 * ```
		 */
		class EngineIpi : public EngineBase {
			friend class ::EngineIpIntelligenceTests;
		public:
			/**
			 * @name Constructors
			 * @{
			 */

			 /**
			  * @copydoc Common::EngineBase::EngineBase
			  * The data set is constructed from the file provided.
			  * @param fileName path to the file containing the data file
			  * to load
			  */
			EngineIpi(
				const char *fileName,
				ConfigIpi *config,
				RequiredPropertiesConfig *properties);

			/**
			 * @copydoc Common::EngineBase::EngineBase
			 * The data set is constructed from the file provided.
			 * @param fileName path to the file containing the data file to
			 * load
			 */
			EngineIpi(
				const string &fileName,
				ConfigIpi *config,
				RequiredPropertiesConfig *properties);

			/**
			 * @copydoc Common::EngineBase::EngineBase
			 * The data set is constructed from data stored in memory
			 * described by the data and length parameters.
			 * @param data pointer to the memory containing the data set
			 * @param length size of the data in memory
			 */
			EngineIpi(
				void *data,
				fiftyoneDegreesFileOffset length,
				ConfigIpi *config,
				RequiredPropertiesConfig *properties);

			/**
			 * @copydoc Common::EngineBase::EngineBase
			 * The data set is constructed from data stored in memory
			 * described by the data and length parameters.
			 * @param data pointer to the memory containing the data set
			 * @param length size of the data in memory
			 */
			EngineIpi(
				unsigned char data[],
				fiftyoneDegreesFileOffset length,
				ConfigIpi *config,
				RequiredPropertiesConfig *properties);

			/**
			 * @}
			 * @name Engine Methods
			 * @{
			 */

			/**
			 * Processes the evidence provided and returns the results.
			 * @param evidence to process. The keys in getKeys() will be
			 * the only ones considered by the engine.
			 * @return a new results instance with the values for all requested
			 * properties
			 */
			ResultsIpi* process(EvidenceIpi *evidence);

			/**
			 * Processes the IP address string provided and returns the results.
			 * This will auto detect the type of IP being used
			 * @param ipAddress the IP address string to process
			 * @return a new results instance with the values for all requested
			 * properties
			 */
			ResultsIpi* process(const char *ipAddress);

			/**
			 * Processes the raw IP address byte array and returns the results.
			 * This will rely on user input length and IP type to determine
			 * the amount of data that it need to process.
			 * If the data is provided is smaller than the expected size for
			 * the IP type (4 for ipv4 and 16 for ipv6) and incorrect format
			 * exception will be thrown.
			 * @param ipAddress the IP address byte array to process
			 * @param length the size of the byte array
			 * @param type of the IP
			 * @return a new results instance with the value for all requested
			 * properties
			 */
			ResultsIpi *process(
				unsigned char ipAddress[],
				long length,
				fiftyoneDegreesIpType type);

			/**
			 * Processes the evidence as process(EvidenceIpi*) but evaluates
			 * only the graphs needed by the required properties whose indexes
			 * are supplied. NULL or a negative count evaluates every graph. A
			 * count of zero evaluates none. A property whose graph was not
			 * evaluated has no value with the NULL_PROFILE reason, except that
			 * a property which is mandatory with a default value reads as that
			 * default. The native mask is 32 bits, so components beyond index
			 * 31 are always evaluated. Indexes are positions in
			 * getRequiredProperties(), which move when a refresh loads a data
			 * file that gains or loses a required property, so look them up
			 * again after a refresh.
			 * @param evidence to process
			 * @param requiredPropertyIndexes the caller will read
			 * @param requiredPropertyIndexesCount entries in the array
			 * @return a new results instance, owned by the caller
			 */
			ResultsIpi* process(
				EvidenceIpi *evidence,
				const int *requiredPropertyIndexes,
				int requiredPropertyIndexesCount);

			/**
			 * Processes the IP address string as process(const char*) but
			 * evaluates only the graphs needed by the required properties
			 * whose indexes are supplied. See the evidence overload for the
			 * rules.
			 * @param ipAddress the IP address string to process
			 * @param requiredPropertyIndexes the caller will read
			 * @param requiredPropertyIndexesCount entries in the array
			 * @return a new results instance, owned by the caller
			 */
			ResultsIpi* process(
				const char *ipAddress,
				const int *requiredPropertyIndexes,
				int requiredPropertyIndexesCount);

			/**
			 * Processes the raw IP address byte array as
			 * process(unsigned char[], long, fiftyoneDegreesIpType) but
			 * evaluates only the graphs needed by the required properties
			 * whose indexes are supplied. See the evidence overload for the
			 * rules.
			 * @param ipAddress the IP address byte array to process
			 * @param length the size of the byte array
			 * @param type of the IP
			 * @param requiredPropertyIndexes the caller will read
			 * @param requiredPropertyIndexesCount entries in the array
			 * @return a new results instance, owned by the caller
			 */
			ResultsIpi *process(
				unsigned char ipAddress[],
				long length,
				fiftyoneDegreesIpType type,
				const int *requiredPropertyIndexes,
				int requiredPropertyIndexesCount);

			/**
			 * @}
			 * @name Common::EngineBase Implementation
			 * @{
			 */

			void refreshData() const override;

			void refreshData(const char *fileName) const override;

			void refreshData(void *data, fiftyoneDegreesFileOffset length) const override;

			void refreshData(unsigned char data[], fiftyoneDegreesFileOffset length) const override;

			ResultsBase* processBase(EvidenceBase *evidence) const override;

			Date getPublishedTime() const override;

			Date getUpdateAvailableTime() const override;

			string getDataFilePath() const override;

			string getDataFileTempPath() const override;

			string getProduct() const override;

			string getType() const override;

			/**
			 * @}
			 */
		protected:
			/**
			 * Initialise the header keys which are used by this engine.
			 * These are the pieces of evidence which should be passed in if
			 * available.
			 * @param uniqueHeaders to get the keys from
			 */
			void initHttpHeaderKeys(fiftyoneDegreesHeaders *uniqueHeaders) override;

			/**
			 * Derive the evidence keys for a set of unique headers without
			 * touching the engine's own key list, so that a refresh can
			 * compare the result against the keys already in use.
			 * @param uniqueHeaders to get the keys from
			 * @return the evidence keys for the headers, in header order
			 */
			vector<string> buildHttpHeaderKeys(
				fiftyoneDegreesHeaders *uniqueHeaders) const;

		private:
			void initMetaData();

			/**
			 * Rebuild the evidence keys from the unique headers of the data
			 * set currently held by the resource manager. Called after every
			 * successful refresh, so that getKeys() describes the data set in
			 * use rather than the one the engine was built from.
			 *
			 * The new list is built before anything is modified and installed
			 * with a single swap, and the keys are left untouched when the
			 * reloaded data set declares the same headers. A refresh which
			 * does change the headers still mutates the list returned by
			 * getKeys(), which is not synchronised against concurrent
			 * readers, so a caller must not hold that pointer across a
			 * refresh.
			 */
			void reloadKeys() const;

			void init();

			void* copyData(void *data, fiftyoneDegreesFileOffset length) const;
		};
	}
}

#endif
