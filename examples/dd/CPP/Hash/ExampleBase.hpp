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
 
#ifndef FIFTYONE_DEGREES_EXAMPLE_BASE_CPLUSPLUS_INCLUDED
#define FIFTYONE_DEGREES_EXAMPLE_BASE_CPLUSPLUS_INCLUDED

// Include ExmapleBase.h before others as it includes Windows 'crtdbg.h'
// which requires to be included before 'malloc.h'.
#include "../../C/Hash/ExampleBase.h"
#include "../../../../src/dd/hash/EngineHash.hpp"
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include "../../../../src/common/textfile.h"

#define THREAD_COUNT 4

static const char *dataDir = "data/device-detection";

static const char *dataFileName = "51Degrees-LiteV4.1.hash";

static const char *userAgentFileName = "20000 User Agents.csv";

using std::cout;
using std::thread;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace FiftyoneDegrees {
	namespace Examples {
		/**
		 * C++ Hash engine examples.
		 */
		namespace Hash {
			/**
			 * Base class extended by all Hash examples.
			 */
			class ExampleBase {
			public:

				/**
				 * Construct a new instance of the example to be run using the
				 * data pointer provided.
				 * @param data pointer to the data set in memory
				 * @param length of the data in bytes
				 * @param config to configure the engine with, which the
				 * example deletes, even if the engine cannot be created
				 */
				ExampleBase(
					byte *data, 
					long length, 
					DeviceDetection::Hash::ConfigHash *config);

				/**
				 * Construct a new instance of the example to be run using the
				 * data file provided.
				 * @param dataFilePath path to the data file to use
				 */
				ExampleBase(string dataFilePath);

				/**
				 * Construct a new instance of the example to be run using the
				 * data file and configuration provided.
				 * @param dataFilePath path to the data file to use
				 * @param config to configure the engine with, which the
				 * example deletes, even if the engine cannot be created
				 */
				ExampleBase(
					string dataFilePath, 
					DeviceDetection::Hash::ConfigHash *config);

				/**
				 * Dispose of anything created with the example.
				 */
				virtual ~ExampleBase();

				/**
				 * Run the example.
				 */
				virtual void run() = 0;

				/** Example mobile User-Agent string */
				static const char* mobileUserAgent;

				/** Example desktop User-Agent string */
				static const char* desktopUserAgent;

				/** Example UACH Platform value */
				static const char* uachPlatform;

				/** Example UACH Platform Version value */
				static const char* uachPlatformVersion;

				/** Example media hub User-Agent string */
				static const char* mediaHubUserAgent;

				/**
				 * Reports the status of the data file initialization.
				 * @param status associated with the initialisation
				 * @param fileName used for initialisation
				 */
				static void reportStatus(
					fiftyoneDegreesStatusCode status,
					const char* fileName);

				/**
				 * Finds a file in the data folder, and downloads it first
				 * when it is missing and the data repository links to it.
				 * @param dataFolderName name of the folder to look in
				 * @param fileName name of the file to find
				 * @param destination memory to write the path of the file to
				 * @param size of the destination in bytes
				 * @return the status of the last search
				 */
				static fiftyoneDegreesStatusCode getFilePath(
					const char* dataFolderName,
					const char* fileName,
					char* destination,
					size_t size);

			protected:
				/**
				 * True when the data file has the property. The free Lite data
				 * file leaves out the properties only the Enterprise data file
				 * has. The data file's own list is read, so the answer does not
				 * depend on the required properties.
				 * @param name of the property as the data file writes it
				 * @return true when the data file has the property
				 */
				bool hasProperty(const char *name) const;

				/**
				 * Says of each property the engine was asked for that the data
				 * file does not have that it needs the Enterprise data file, one
				 * line each.
				 */
				void reportEnterpriseProperties() const;

				/**
				 * On the free Lite data file, says what the Enterprise data
				 * file adds and where to ask for a copy.
				 * @param contactUs link to the contact page, tagged with the
				 * example
				 */
				void offerEnterprise(const char *contactUs) const;

				/**
				 * State containing the states for all threads running in a
				 * multi-threaded example .
				 */
				class SharedState {
				public:
					/**
					 * Construct a new shared state instance.
					 * @param engine pointer to the engine the threads should
					 * use
					 * @param userAgentFilePath path to the User-Agents CSV
					 */
					SharedState(EngineHash *engine, string userAgentFilePath);

					/**
					 * Starts threads that run the #processUserAgentsMulti
					 * method.
					 */
					void startThreads();

					/**
					 * Joins the threads and frees the memory occupied by the
					 * threads.
					 */
					void joinThreads();

					/**
					 * Processes all the User-Agents in the file named in the
					 * shared state using the engine in the state using a single
					 * thread, and outputs the hash of the results.
					 */
					void processUserAgentsSingle();

					/**
					 * Calls the #processUserAgentsSingle method with the 
					 * state, then increments the number of threads finished 
					 * counter.
					 * @param state pointer to a ExampleBase::SharedState to 
					 * use
					 */
					static void processUserAgentsMulti(void *state);

					EngineHash *engine; /**< Pointer to the engine */
					volatile long threadsFinished; /**< Number of threads that
												   have finished their
												   processing */
					string userAgentFilePath; /**< Path to the User-Agents to
											  process */
					thread threads[THREAD_COUNT]; /**< Pointers to the running
												  threads */
				};

				/**
				 * State for a single thread carrying out processing in order
				 * to store a hash of the results.
				 */
				class ThreadState {
				public:
					/**
					 * Construct a new thread state instance.
					 * @param engine pointer to the engine the thread should
					 * use
					 */
					ThreadState(EngineHash *engine);
					EngineHash *engine; /**< Pointer to the engine */
					int hashCode; /**< Running hash code for the processing
								  being carried out. This is used to verify the
								  work carried out */
				};

				/**
				 * Get the hash code for all the values stored in the results
				 * instance.
				 * @param results instance to hash
				 * @return hash code for the results values
				 */
				static unsigned long getHashCode(
					DeviceDetection::Hash::ResultsHash* results) {
					unsigned long hashCode = 0;
					Common::Value<string> value;

					for (const string &name : results->getProperties()) {
						value = results->getValueAsString(name);
						if (value.hasValue()) {
							hashCode ^= generateHash(
								(unsigned char*)(value.getValue().c_str()));
						}
					}
					return hashCode;
				}

				/**
				 * Processes a User-Agent string and hashes the results, adding
				 * to the hash in the thread state provided.
				 * @param userAgent the User-Agent to hash
				 * @param state pointer to a ExampleBase::ThreadState
				 */
				static void processUserAgent(const char *userAgent, void *state);

				/** Configuration for the Engine */
				std::unique_ptr<DeviceDetection::Hash::ConfigHash> config;
				/** Properties to initialise the Engine with */
				std::unique_ptr<RequiredPropertiesConfig> properties;
				/** Hash Engine used for the example. Declared last, so it is
				 * freed before the configuration and properties it uses. */
				std::unique_ptr<EngineHash> engine;

			private:
				/**
				 * Get the hash code for a string of characters.
				 * @param value the string to hash
				 * @return hash code for the string
				 */
				static unsigned long generateHash(unsigned char *value);
			};
		}
	}
}

#endif