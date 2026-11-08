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

#ifndef FIFTYONE_DEGREES_IPI_INCLUDED
#define FIFTYONE_DEGREES_IPI_INCLUDED

/**
 * @ingroup FiftyOneDegreesIpIntelligence
 * @defgroup FiftyOneDegreesIpIntelligenceApi IpIntelligence
 *
 * All the functions specific to the IP Intelligence API.
 * @{
 */

#if !defined(DEBUG) && !defined(_DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "../common/data.h"
#include "../common/exceptions.h"
#include "../common/threading.h"
#include "../common/file.h"
#include "../common/collection.h"
#include "../common/evidence.h"
#include "../common/list.h"
#include "../common/resource.h"
#include "../common/properties.h"
#include "../common/status.h"
#include "../common/date.h"
#include "../common/pool.h"
#include "../common/component.h"
#include "../common/property.h"
#include "../common/value.h"
#include "../common/profile.h"
#include "../common/overrides.h"
#include "../common/config.h"
#include "../common/dataset.h"
#include "../common/array.h"
#include "../common/results.h"
#include "../common/float.h"
#include "../common/stringBuilder.h"
#include "../common/weightedItem.h"
#include "graph/graph.h"

/**
 * Default value for the concurrency used in the default configurations, where
 * 0 is one operation for each processor the system has.
 */
#ifndef FIFTYONE_DEGREES_CACHE_CONCURRENCY
#ifndef FIFTYONE_DEGREES_NO_THREADING
#define FIFTYONE_DEGREES_CACHE_CONCURRENCY 0
#else
#define FIFTYONE_DEGREES_CACHE_CONCURRENCY 1
#endif
#endif

/**
 * Default value for the string cache size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_STRING_CACHE_SIZE
#define FIFTYONE_DEGREES_STRING_CACHE_SIZE 10000
#endif
/**
 * Default value for the string cache loaded size used in the default
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_STRING_LOADED
#define FIFTYONE_DEGREES_STRING_LOADED true
#endif
/**
 * Default value for the graphs cache size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_IP_GRAPHS_CACHE_SIZE
#define FIFTYONE_DEGREES_IP_GRAPHS_CACHE_SIZE 1000
#endif
/**
 * Default value for the graphs cache loaded size used in the default 
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_IP_GRAPHS_LOADED
#define FIFTYONE_DEGREES_IP_GRAPHS_LOADED true
#endif
/**
 * Default value for the graphs cache size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE
#define FIFTYONE_DEGREES_IP_GRAPH_CACHE_SIZE 50000
#endif
/**
 * Default value for the graphs cache loaded size used in the default 
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_IP_GRAPH_LOADED
#define FIFTYONE_DEGREES_IP_GRAPH_LOADED true
#endif
/**
 * Default value for the profile groups cache size used in the default 
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_PROFILE_GROUPS_CACHE_SIZE
#define FIFTYONE_DEGREES_PROFILE_GROUPS_CACHE_SIZE 50000
#endif
/**
 * Default value for the profile groups cache loaded size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_PROFILE_GROUPS_LOADED
#define FIFTYONE_DEGREES_PROFILE_GROUPS_LOADED false
#endif
/**
 * Default value for the profile cache size used in the default collection
 * configuration. The Balanced preset reads the profile of each result through
 * this cache, so it holds enough of them that most reads find their profile
 * there.
 */
#ifndef FIFTYONE_DEGREES_PROFILE_CACHE_SIZE
#define FIFTYONE_DEGREES_PROFILE_CACHE_SIZE 20000
#endif
/**
 * Default value for the profile cache loaded size used in the default
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_PROFILE_LOADED
#define FIFTYONE_DEGREES_PROFILE_LOADED false
#endif
/**
 * Default value for the value cache size used in the default collection
 * configuration. The Balanced preset reads the values of every property of a
 * lookup through this cache, so it holds enough of them that most reads find
 * their value there.
 */
#ifndef FIFTYONE_DEGREES_VALUE_CACHE_SIZE
#define FIFTYONE_DEGREES_VALUE_CACHE_SIZE 100000
#endif
/**
 * Default value for the value cache loaded size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_VALUE_LOADED
#define FIFTYONE_DEGREES_VALUE_LOADED false
#endif
/**
 * Default value for the property cache size used in the default collection
 * configuration.
 */
#ifndef FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE
#define FIFTYONE_DEGREES_PROPERTY_CACHE_SIZE 0
#endif
/**
 * Default value for the property cache loaded size used in the default
 * collection configuration.
 */
#ifndef FIFTYONE_DEGREES_PROPERTY_LOADED
#define FIFTYONE_DEGREES_PROPERTY_LOADED true
#endif

/**
 * DATA STRUCTURES
 */

/** Dataset header containing information about the dataset. */
#pragma pack(push, 1)
typedef struct fiftyone_degrees_ipi_dataset_header_t {
	const int32_t versionMajor; /**< Major version of the data file loaded */
	const int32_t versionMinor; /**< Minor version of the data file loaded */
	const int32_t versionBuild; /**< Build version of the data file loaded */
	const int32_t versionRevision; /**< Revision version of the data file 
								   loaded */
	const byte tag[16]; /**< Unique data file tag */
	const byte exportTag[16]; /**< Tag identifying the data file export */
	const int32_t copyrightOffset; /**< Offset of the copyright string in the 
								   strings collection */
	const int16_t age; /**< Age of the data set format */
	const int32_t profilesOffsetShift; /**< Number of bits to shift a stored
							profile offset, or the profiles collection length,
							left to convert it to bytes. Zero where profile
							offsets are byte positions. Occupies a previously
							reserved field always written as zero. */
	const int32_t nameOffset; /**< Offset of the data file name in the strings 
							  collection */
	const int32_t formatOffset; /**< Offset of the data file format in the 
								strings collection */
	const fiftyoneDegreesDate published; /**< Date when the data file was 
										 published */
	const fiftyoneDegreesDate nextUpdate; /**< Date when the next data file 
										  will be available */
	const fiftyoneDegreesCollectionHeader strings; /**< Size and location of
												   the strings collection */
	const fiftyoneDegreesCollectionHeader components; /**< Size and location of
													  the components 
													  collection */
	const fiftyoneDegreesCollectionHeader maps; /**< Size and location of the
												maps collection */
	const fiftyoneDegreesCollectionHeader properties; /**< Size and location of
													  the properties 
													  collection */
	const fiftyoneDegreesCollectionHeader values; /**< Size and location of the
												  values collection */
	const fiftyoneDegreesCollectionHeader profiles; /**< Size and location of
													the profiles collection */
	const fiftyoneDegreesCollectionHeader graphs; /**< Headers for component
												  graphs */
	const fiftyoneDegreesCollectionHeader profileGroups; /**< Size and
														  location of the
														  profile group offsets
														  collection */
	const fiftyoneDegreesCollectionHeader propertyTypes; /**< Size and location of
													     the propertyTypes
													     collection */
	const fiftyoneDegreesCollectionHeader profileOffsets; /**< Size and
														  location of the
														  profile offsets
														  collection */
} fiftyoneDegreesDataSetIpiHeader;
#pragma pack(pop)

/**
 * IP Intelligence specific configuration structure.
 */
typedef struct fiftyone_degrees_config_ipi_t {
	fiftyoneDegreesConfigBase b; /**< Base configuration */
	fiftyoneDegreesCollectionConfig strings; /**< Strings collection config */
	fiftyoneDegreesCollectionConfig components; /**< Components collection
												config */
	fiftyoneDegreesCollectionConfig maps; /**< Maps collection config */
	fiftyoneDegreesCollectionConfig properties; /**< Properties collection
												config */
	fiftyoneDegreesCollectionConfig values; /**< Values collection config */
	fiftyoneDegreesCollectionConfig profiles; /**< Profiles collection config 
											  */
	fiftyoneDegreesCollectionConfig graphs; /**< Graphs config */
	fiftyoneDegreesCollectionConfig profileGroups; /**< profileGroups
												   collection config */
	fiftyoneDegreesCollectionConfig profileOffsets; /**< ProfileOffsets
													collection config */
	fiftyoneDegreesCollectionConfig propertyTypes; /**< Property types collection
												   config */
	fiftyoneDegreesCollectionConfig graph; /**< Config for each graph */
} fiftyoneDegreesConfigIpi;

/**
 * Data set structure which contains the base dataset structure. This
 * acts as a wrapper and is used in the dataset structure for IP 
 * intelligence to be compatible with some of the common library
 * APIs.
 */
typedef struct fiftyone_degrees_dataset_ipi_base_t {
	fiftyoneDegreesDataSetBase b; /**< Base structure members */
} fiftyoneDegreesDataSetIpiBase;

/**
 * Data set structure containing all the components used for IP intelligence.
 * This should predominantly be used through a #fiftyoneDegreesResourceManager
 * pointer to maintain a safe reference. If access the data set is needed then
 * a safe reference can be fetched and released with the
 * #fiftyoneDegreesDataSetIpiGet and #fiftyoneDegreesDataSetIpiRelease
 * methods.
 */
typedef struct fiftyone_degrees_dataset_ipi_t {
	fiftyoneDegreesDataSetIpiBase b; /**< Base data set */
	const fiftyoneDegreesDataSetIpiHeader header; /**< Dataset header */
	const fiftyoneDegreesConfigIpi config; /**< Copy of the configuration */
	fiftyoneDegreesCollection *strings; /**< Collection of all strings */
	fiftyoneDegreesCollection *components; /**< Collection of all components */
	fiftyoneDegreesList componentsList; /**< List of component items from the
										components collection */
	bool *componentsAvailable; /**< Array of flags indicating if there are
							   any properties available for the component with
							   the matching index in componentsList */
	uint32_t componentsAvailableCount; /**< Number of components with
									   properties */
	fiftyoneDegreesCollection *maps; /**< Collection data file maps */
	fiftyoneDegreesCollection *properties; /**< Collection data file properties
										   */
	fiftyoneDegreesCollection *values; /**< Collection data file values */
	fiftyoneDegreesCollection *profiles; /**< Collection data file profiles */
	fiftyoneDegreesCollection *graphs; /**< Collection of graph infos used to
									   create the array of graphs */
	fiftyoneDegreesCollection *profileGroups; /**< Collection of all profile 
											  groups where more than one 
											  profile is required with a weight
											  */
	fiftyoneDegreesCollection *propertyTypes; /**< Collection data file properties
											  */
	fiftyoneDegreesCollection *profileOffsets; /**< Collection of all offsets
											   to profiles in the profiles
											   collection */
	fiftyoneDegreesIpiCgArray* graphsArray; /**< Array of graphs from 
											collection */
} fiftyoneDegreesDataSetIpi;


/**
 * Backward compatibility typedef for ProfilePercentage.
 * @deprecated Use fiftyoneDegreesWeightedItem instead.
 */
typedef fiftyoneDegreesWeightedItem fiftyoneDegreesProfilePercentage;

/**
 * Backward compatibility typedef for IpiList.
 * @deprecated Use fiftyoneDegreesWeightedItemList instead.
 */
typedef fiftyoneDegreesWeightedItemList fiftyoneDegreesIpiList;

/**
 * Singular IP address result returned by an IP address lookup method.
 */
typedef struct fiftyone_degrees_result_ipi_t {
	fiftyoneDegreesIpType type; /**< The version of the IP */
	fiftyoneDegreesIpiCgResult graphResult; /**< The result of
												 graph evaluation */
	fiftyoneDegreesIpAddress targetIpAddress; /**< The target IP address
											  to find a matching range for */
	int32_t componentIndex; /**< Index in the components list of the
							component the result relates to, or -1 if the
							result has not been populated */
	const byte* keptProfile; /**< The profile of the result if it has been kept
							 for the length of the lookup, otherwise NULL */
} fiftyoneDegreesResultIpi;

/**
 * Macro defining the common members of an Ipi result.
 */
#define FIFTYONE_DEGREES_RESULTS_IPI_MEMBERS \
	fiftyoneDegreesResultsBase b; \
	fiftyoneDegreesCollectionItem propertyItem; \
	fiftyoneDegreesIpiList values; \
	byte* hasValuesCache; /**< Cached outcomes of the profile walks used to \
						  check if a property has values. Indexed by required \
						  property index. Reset when the results are \
						  repopulated. */ \
	uint32_t hasValuesCacheCount; /**< Number of entries in hasValuesCache */ \
	byte* keptProfiles; /**< Copies of profiles read through a cache, kept for \
						the length of a lookup so that each property does not \
						read the cache again. One slot for each result, or \
						NULL if the profiles are not read through a cache. */

FIFTYONE_DEGREES_ARRAY_TYPE(
	fiftyoneDegreesResultIpi,
	FIFTYONE_DEGREES_RESULTS_IPI_MEMBERS)

/**
 * Array of Ipi results used to easily access and track the size of the
 * array.
 *
 * The results are used by one thread at a time, because reading a value
 * writes into the results and, when no properties were named, the first read
 * of a component walks its graph.
 * Different results can be used on different threads at once, and the
 * resource manager can be shared, unless the library is compiled with
 * FIFTYONE_DEGREES_NO_THREADING.
 */
typedef fiftyoneDegreesResultIpiArray fiftyoneDegreesResultsIpi;

/**
 * Define the IP range structure
 */
#define FIFTYONE_DEGREES_IP_RANGE(v, s) \
typedef struct fiftyone_degrees_dataset_ip_v##v##_range_t { \
	byte start[s]; /**< the start of the range in byte array format */ \
	int32_t profileOffsetIndex; /**< The index of set the matching profile offset/group */ \
} fiftyoneDegreesIpv##v##Range;

/**
 * Define the ip roots structure
 */
FIFTYONE_DEGREES_IP_RANGE(4, 4)

/**
 * Define the ip nodes structure
 */
FIFTYONE_DEGREES_IP_RANGE(6, 16)

/**
 * Index of a profile in a profile groups item
 * This include the component Index to indicate in which component
 * it belong to.
 */
typedef struct fiftyone_degrees_combination_profile_index_t {
	int32_t componentIndex; /**< Index of the component based on components list */
	int32_t profileIndex; /**< Index of the profile index for the associated component */
} fiftyoneDegreesCombinationProfileIndex;

/**
 * Index of a profile in a profile groups item of a result
 * This include the component and result Indices to indicate in which
 * component the profile belong to in which result. 
 */
typedef struct fiftyone_degrees_result_profile_index_t {
	int16_t resultIndex;
	fiftyoneDegreesCombinationProfileIndex componentProfileIndex;
} fiftyoneDegreesResultProfileIndex;

/**
 * IP INTELLIGENCE CONFIGURATIONS
 */

/**
 * Configuration to be used where the data set is being created using a buffer
 * in memory and concepts like caching are not required. The concurrency
 * setting is ignored as there are no critical sections with this configuration.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiInMemoryConfig;

/**
 * Highest performance configuration that reads the data file. Loads all the
 * data into memory, unless the required properties name some but not all of
 * the properties in the data file. Then only what they read is loaded, being
 * the graphs, profile offsets, profile groups and profiles of their
 * components, their values and the strings of those values, and anything else
 * reads from the file. A lookup that reads the named properties reads nothing
 * from the file. A connection to the data file is kept, with as many readers
 * as the highest concurrency setting.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiHighPerformanceConfig;

/**
 * Low memory configuration. A connection is maintained to the source data file
 * used to build the data set and used to load data into memory when required.
 * No caching is used resulting in the lowest memory footprint at the expense
 * of performance. The concurrency of each collection must be set to the
 * maximum number of concurrent operations to optimize file reads.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiLowMemoryConfig;

/**
 * Uses caching to balance memory usage and performance. A connection is
 * maintained to the source data file to load data into caches when required.
 * As the cache is loaded, memory will increase until the cache capacity is
 * reached. The concurrency of each collection must be set to the maximum
 * number of concurrent operations to optimize file reads. This is the default
 * configuration.
 *
 * The strings, the graphs, the profile offsets, the property types and the
 * default value of each required property are held in memory. The values,
 * profiles and profile groups are read through caches, and the profile of each
 * result is copied the first time a lookup reads it, so that each property
 * does not read the cache again. Naming the required properties does not
 * change what is held.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiBalancedConfig;

/**
 * Balanced configuration modified to create a temporary file copy of the
 * source data file to avoid locking the source data file.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiBalancedTempConfig;

/**
 * Default configuration. This configures the data set to not create
 * a temp file.
 */
EXTERNAL_VAR fiftyoneDegreesConfigIpi fiftyoneDegreesIpiDefaultConfig;


/**
 * EXTERNAL METHODS
 */

/**
 * Gets a safe reference to the IP Intelligencedata set from the resource manager.
 * Fetching through this method ensures that the data set it not freed or moved
 * during the time it is in use.
 * The data set returned by this method should be released with the
 * #fiftyoneDegreesDataSetIpiRelease method.
 * @param manager the resource manager containing a IP intelligence data set initialised
 * by one of the IP intelligence data set init methods
 * @return a fixed pointer to the data set in manager
 */
EXTERNAL fiftyoneDegreesDataSetIpi* fiftyoneDegreesDataSetIpiGet(
	fiftyoneDegreesResourceManager* manager);

/**
 * Release the reference to a data set returned by the
 * #fiftyoneDegreesDataSetIpiGet method. Doing so tell the resource manager
 * linked to the data set that it is no longer being used.
 * @param dataSet pointer to the data set to release
 */
EXTERNAL void fiftyoneDegreesDataSetIpiRelease(fiftyoneDegreesDataSetIpi* dataset);

/**
 * Gets the total size in bytes which will be allocated when intialising a
 * IP Intelligence resource and associated manager with the same parameters. If any of
 * the configuration options prevent the memory from being constant (i.e. more
 * memory may be allocated at process time) then zero is returned.
 * @param config configuration for the operation of the data set, or NULL if
 * default configuration is required
 * @param properties the properties that will be consumed from the data set, or
 * NULL if all available properties in the IP Intelligence data file should be available
 * for consumption
 * @param fileName the full path to a file with read permission that contains
 * the IP Intelligence data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h. Holds the status if the data set could
 * not be initialised, for example from a missing or rejected file, and the
 * number returned then means nothing.
 * @return the total number of bytes needed to initialise a IP Intelligence resource
 * and associated manager with the configuration provided or zero
 */
EXTERNAL size_t fiftyoneDegreesIpiSizeManagerFromFile(
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	const char* fileName,
	fiftyoneDegreesException* exception);

/**
 * Initialises the resource manager with an IP intelligence data set resource populated
 * from the IP Intelligence data file referred to by fileName. Configures the data set
 * to operate using the configuration set in the config, collection and
 * properties.
 * @param manager the resource manager to manager the share data set resource
 * @param config configuration for the operation of the data set, or NULL if
 * default configuration is required
 * @param properties the properties that will be consumed from the data set, or
 * NULL if all available properties in the IP Intelligence data file should be available
 * for consumption
 * @param fileName the full path to a file with read permission that contains
 * the IP Intelligence data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the status associated with the data set resource assign to the
 * resource manager. Any value other than #FIFTYONE_DEGREES_STATUS_SUCCESS
 * means the data set was not created and the resource manager can not be used.
 */
EXTERNAL fiftyoneDegreesStatusCode fiftyoneDegreesIpiInitManagerFromFile(
	fiftyoneDegreesResourceManager* manager,
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	const char* fileName,
	fiftyoneDegreesException* exception);

/**
 * Gets the total size in bytes which will be allocated when intialising a
 * IP Intelligence resource and associated manager with the same parameters. If any of
 * the configuration options prevent the memory from being constant (i.e. more
 * memory may be allocated at process time) then zero is returned.
 * @param config configuration for the operation of the data set, or NULL if
 * default configuration is required
 * @param properties the properties that will be consumed from the data set, or
 * NULL if all available properties in the IP Intelligence data file should be available
 * for consumption
 * @param memory pointer to continuous memory containing the IP intelligence data set,
 * which stays with the caller
 * @param size the number of bytes that make up the IP intelligence data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h. Holds the status if the data set could
 * not be initialised, for example from memory that is short or rejected, and
 * the number returned then means nothing.
 * @return the total number of bytes needed to initialise a IP Intelligence resource
 * and associated manager with the configuration provided or zero
 */
EXTERNAL size_t fiftyoneDegreesIpiSizeManagerFromMemory(
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	void* memory,
	fiftyoneDegreesFileOffset size,
	fiftyoneDegreesException* exception);

/**
 * Initialises the resource manager with a IP Intelligence data set resource populated
 * from the IP intelligence data set pointed to by the memory parameter. Configures the
 * data set to operate using the configuration set in the config and properties.
 * @param manager the resource manager to manager the share data set resource
 * @param config configuration for the operation of the data set, or NULL if
 * default configuration is required
 * @param properties the properties that will be consumed from the data set, or
 * NULL if all available properties in the IP intelligence data file should be available
 * for consumption
 * @param memory pointer to continuous memory containing the IP Intelligence data set.
 * If the data set is not created the memory stays with the caller, even if the
 * configuration says to free it
 * @param size the number of bytes that make up the IP Intelligence data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the status associated with the data set resource assign to the
 * resource manager. Any value other than #FIFTYONE_DEGREES_STATUS_SUCCESS
 * means the data set was not created and the resource manager can not be used.
 */
EXTERNAL fiftyoneDegreesStatusCode fiftyoneDegreesIpiInitManagerFromMemory(
	fiftyoneDegreesResourceManager* manager,
	fiftyoneDegreesConfigIpi* config,
	fiftyoneDegreesPropertiesRequired* properties,
	void* memory,
	fiftyoneDegreesFileOffset size,
	fiftyoneDegreesException* exception);

/**
 * Read a profile groups item from the file collection provided and 
 * store in the data pointer. This method is used when creating a collection
 * from file.
 * @param file collection to read from
 * @param offset of the profile groups in the collection
 * @param data to store the resulting profile groups in
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return pointer to the component allocated within the data structure
 */
EXTERNAL void* fiftyoneDegreesProfileCombinationReadFromFile(
	const fiftyoneDegreesCollectionFile* file,
	uint32_t offset,
	fiftyoneDegreesData* data,
	fiftyoneDegreesException* exception);

/**
 * Reload the data set being used by the resource manager using the data file
 * location specified. When initialising the data, the configuration that
 * manager was first created with is used.
 *
 * If the new data file is successfully initialised, the current data set is
 * replaced The old data will remain in memory until the last
 * #fiftyoneDegreesResultsIpi which contain a reference to it are released.
 *
 * This method is defined by the #FIFTYONE_DEGREES_DATASET_RELOAD macro.
 * @param manager pointer to the resource manager to reload the data set for
 * @param fileName path to the new data file
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the status associated with the data set reload. Any value other than
 * #FIFTYONE_DEGREES_STATUS_SUCCESS means the data set was not reloaded and
 * the current one stays in use
 */
EXTERNAL fiftyoneDegreesStatusCode fiftyoneDegreesIpiReloadManagerFromFile(
	fiftyoneDegreesResourceManager* manager,
	const char* fileName,
	fiftyoneDegreesException* exception);

/**
 * Reload the data set being used by the resource manager using a data file
 * loaded into contiguous memory. When initialising the data, the configuration
 * that manager was first created with is used.
 *
 * If the data passed in is successfully initialised, the current data set is
 * replaced The old data will remain in memory until the last
 * #fiftyoneDegreesResultsIpi which contain a reference to it are released.
 *
 * This method is defined by the #FIFTYONE_DEGREES_DATASET_RELOAD macro.
 * @param manager pointer to the resource manager to reload the data set for
 * @param source pointer to the memory location where the new data file is
 *               stored
 * @param length of the data in memory
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the status associated with the data set reload. Any value other than
 * #FIFTYONE_DEGREES_STATUS_SUCCESS means the data set was not reloaded and
 * the current one stays in use
 */
EXTERNAL fiftyoneDegreesStatusCode fiftyoneDegreesIpiReloadManagerFromMemory(
	fiftyoneDegreesResourceManager* manager,
	void* source,
	fiftyoneDegreesFileOffset length,
	fiftyoneDegreesException* exception);

/**
 * Reload the data set being used by the resource manager using the data file
 * location which was used when the manager was created. When initialising the
 * data, the configuration that manager was first created with is used.
 *
 * If the new data file is successfully initialised, the current data set is
 * replaced The old data will remain in memory until the last
 * #fiftyoneDegreesResultsIpi which contain a reference to it are released.
 *
 * This method is defined by the #FIFTYONE_DEGREES_DATASET_RELOAD macro.
 * @param manager pointer to the resource manager to reload the data set for
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the status associated with the data set reload. Any value other than
 * #FIFTYONE_DEGREES_STATUS_SUCCESS means the data set was not reloaded and
 * the current one stays in use
 */
EXTERNAL fiftyoneDegreesStatusCode  
fiftyoneDegreesIpiReloadManagerFromOriginalFile(
	fiftyoneDegreesResourceManager* manager,
	fiftyoneDegreesException* exception);

/**
 * Allocates a results structure containing a reference to the IP Intelligence
 * data set managed by the resource manager provided. The referenced data set
 * will be kept active until the results structure is freed.
 * There can only be one result for per input IP address
 * @param manager pointer to the resource manager which manages a IP 
 * Intelligence data set
 * @return newly created results structure
 */
EXTERNAL fiftyoneDegreesResultsIpi* fiftyoneDegreesResultsIpiCreate(
	fiftyoneDegreesResourceManager* manager);

/**
 * Frees the results structure created by the
 * #fiftyoneDegreesResultsIpiCreate method. When freeing, the reference to
 * the IP Intelligence data set resource is released.
 * @param results pointer to the results structure to release
 */
EXTERNAL void fiftyoneDegreesResultsIpiFree(
	fiftyoneDegreesResultsIpi* results);

/**
 * Process a single byte array format IP Address and populate the IP range 
 * offset in the results structure. The result IP type will need to be checked
 * to determined the version of IP being processed.
 *
 * An IPv4-mapped IPv6 address (::ffff:a.b.c.d) is processed as its IPv4
 * address, and the result's type and target address are those of the IPv4
 * address. This applies to every way of giving an address, including strings
 * and evidence. An IPv4-embedded address from RFC 6052, such as
 * 64:ff9b::a.b.c.d, stays an IPv6 address, because deriving the IPv4 address
 * from it is the job of an address translator (RFC 6052 section 1.3). See
 * https://github.com/51Degrees/specifications/blob/main/ip-intelligence-specification/pipeline-elements/ip-intelligence-on-premise.md#address-forms
 *
 * An address below the lowest key in a graph has no profile for that graph's
 * component and no exception is set. A property of that component has no value
 * with the reason #FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE, unless
 * it is mandatory with a default value, in which case it reads as that default.
 *
 * When the caller named the properties the data set was created with, the
 * graph of each component a named property belongs to is walked here, and a
 * walk that fails sets the exception. Otherwise populating the results walks
 * no graph, and the graph of each component is walked the first time one of
 * its properties is read, against the data set the results hold, as
 * #fiftyoneDegreesResultsIpiGetValues describes.
 * @param results preallocated results structure to populate
 * @param ipAddress byte array to process
 * @param ipAddressLength of the IP address byte array
 * @param type of the ip
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
EXTERNAL void fiftyoneDegreesResultsIpiFromIpAddress(
	fiftyoneDegreesResultsIpi* results,
	const unsigned char* ipAddress,
	size_t ipAddressLength,
	fiftyoneDegreesIpType type,
	fiftyoneDegreesException* exception);

/**
 * Process a single IP Address and populate the IP range offset in the results
 * structure.
 *
 * When the caller named the properties the data set was created with, the
 * graph of each component a named property belongs to is walked here, and a
 * walk that fails sets the exception. Otherwise populating the results walks
 * no graph, and the graph of each component is walked the first time one of
 * its properties is read, against the data set the results hold, as
 * #fiftyoneDegreesResultsIpiGetValues describes.
 * @param results preallocated results structure to populate
 * @param ipAddress string to process
 * @param ipAddressLength of the ipAddress string
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
EXTERNAL void fiftyoneDegreesResultsIpiFromIpAddressString(
	fiftyoneDegreesResultsIpi* results,
	const char* ipAddress,
	size_t ipAddressLength,
	fiftyoneDegreesException* exception);

/**
 * Processes the evidence value pairs in the evidence collection and
 * populates the result in the results structure.
 *
 * When the caller named the properties the data set was created with, the
 * graph of each component a named property belongs to is walked here, and a
 * walk that fails sets the exception. Otherwise populating the results walks
 * no graph, and the graph of each component is walked the first time one of
 * its properties is read, against the data set the results hold, as
 * #fiftyoneDegreesResultsIpiGetValues describes.
 * @param results preallocated results structure to populate containing a
 *                pointer to an initialised resource manager
 * @param evidence to process containing parsed or unparsed values
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
EXTERNAL void fiftyoneDegreesResultsIpiFromEvidence(
	fiftyoneDegreesResultsIpi* results,
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence,
	fiftyoneDegreesException* exception);

/**
 * Gets whether or not the results provided contain valid values for the
 * property index provided.
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results to check
 * @param requiredPropertyIndex index of the property in the names given
 * when the data set was created, which a reload does not change. With no
 * names given it is the index in the available properties, which a reload
 * can change, so look it up from the name for each data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return true if there are valid values in the results for the property index
 * provided
 */
EXTERNAL bool fiftyoneDegreesResultsIpiGetHasValues(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception);

/**
 * Gets the reason why a results does not contain valid values for a given
 * property. 
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results to check
 * @param requiredPropertyIndex index of the property in the names given
 * when the data set was created, which a reload does not change. With no
 * names given it is the index in the available properties, which a reload
 * can change, so look it up from the name for each data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return enum indicating why a valid value cannot be returned by the results
 */
EXTERNAL fiftyoneDegreesResultsNoValueReason fiftyoneDegreesResultsIpiGetNoValueReason(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception);

/**
 * Gets a fuller description of the reason why a value is missing.
 * @param reason enum of the reason for the missing value
 * @return full description for the reason
 */
EXTERNAL const char* fiftyoneDegreesResultsIpiGetNoValueReasonMessage(
	fiftyoneDegreesResultsNoValueReason reason);

/**
 * Populates the list of values in the results instance with value structure
 * instances associated with the required property index. When the results 
 * are released then the value items will be released. There is no need for
 * the caller to release the collection item returned. The 
 * fiftyoneDegreesResultsIpiGetValueString method should be used to get
 * the string representation of the value.
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results structure to release
 * @param requiredPropertyIndex index of the property in the names given
 * when the data set was created, which a reload does not change. With no
 * names given it is the index in the available properties, which a reload
 * can change, so look it up from the name for each data set
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return a pointer to the first value item 
 */
EXTERNAL const fiftyoneDegreesProfilePercentage* fiftyoneDegreesResultsIpiGetValues(
	fiftyoneDegreesResultsIpi* results,
	int requiredPropertyIndex,
	fiftyoneDegreesException* exception);

/**
 * Adds to builder the values associated in the results for the property name.
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results structure to release
 * @param propertyName name of the property to be used with the values
 * @param builder string builder to fill with values
 * @param separator string to be used to separate multiple values if available
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 */
EXTERNAL void fiftyoneDegreesResultsIpiAddValuesString(
	fiftyoneDegreesResultsIpi* results,
	const char* propertyName,
	fiftyoneDegreesStringBuilder *builder,
	const char* separator,
	fiftyoneDegreesException* exception);

/**
 * Sets the buffer the values associated in the results for the property name.
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results structure to release
 * @param propertyName name of the property to be used with the values
 * @param buffer character buffer allocated by the caller
 * @param bufferLength of the character buffer
 * @param separator string to be used to separate multiple values if available
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the number of characters available for values. May be larger than
 * bufferLength if the buffer is not long enough to return the result.
 */
EXTERNAL size_t fiftyoneDegreesResultsIpiGetValuesString(
	fiftyoneDegreesResultsIpi* results,
	const char* propertyName,
	char* buffer,
	size_t bufferLength,
	const char* separator,
	fiftyoneDegreesException* exception);

/**
 * Sets the buffer the values associated in the results for the property name.
 *
 * When no properties were named, the first read of a property of a component
 * walks the component's graph for the results. If the walk fails the exception
 * is set, and the component is not walked again until the results are next
 * populated, so later reads of its properties read as if it had no profile for
 * the address.
 * @param results pointer to the results structure to release
 * @param requiredPropertyIndex index of the property in the names given
 * when the data set was created, which a reload does not change. With no
 * names given it is the index in the available properties, which a reload
 * can change, so look it up from the name for each data set
 * @param buffer character buffer allocated by the caller
 * @param bufferLength of the character buffer
 * @param separator string to be used to separate multiple values if available
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h.
 * @return the number of characters available for values. May be larger than
 * bufferLength if the buffer is not long enough to return the result.
 */
EXTERNAL size_t fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
	fiftyoneDegreesResultsIpi* results,
	const int requiredPropertyIndex,
	char* buffer,
	size_t bufferLength,
	const char* separator,
	fiftyoneDegreesException* exception);

/**
 * Get the network id string from the single result provided. This contains
 * profile ids for all components and their percentages for the matched
 * IP range, delimited by ':'. The profile id percentage pairs are concatenated
 * with the separator character '|'.
 * @param results pointer to the results
 * @param result pointer to the result to get the network id of
 * @param destination pointer to the memory to write the characters to
 * @param size amount of memory allocated to destination
 * @param componentProfileIndex the combination profile index to start
 * constructing the network ID from. Negative values indicate nothing
 * to fetch.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the combination profile index to start from in the next call for
 * fetching the remaining of the network ID. All indices will be -1s there
 * is no more to fetch. The returned format does not contain leading or
 * trailing separator so the separator will need to be added explicitly
 * if more than one call  is required.
 */
EXTERNAL fiftyoneDegreesCombinationProfileIndex 
fiftyoneDegreesIpiGetNetworkIdFromResult(
	fiftyoneDegreesResultsIpi* results,
	fiftyoneDegreesResultIpi* result,
	char* destination,
	size_t size,
	fiftyoneDegreesCombinationProfileIndex componentProfileIndex,
	fiftyoneDegreesException* exception);

/**
 * Get the network id string from the results provided. This contains
 * profile ids for all components and their percentages for the matched
 * IP range, delimited by ':'. The profile id percentage pairs are concatenated
 * with the separator character '|'.
 * @param results pointer to the results
 * @param destination pointer to the memory to write the characters to
 * @param size amount of memory allocated to destination
 * @param resultProfileIndex the profile index to start constructing the network
 * ID from. Negative values indicate nothing to fetch.
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the result profile index to start from in the next call for
 * fetching the remaining of the network ID. All indices will be -1s there
 * is no more to fetch. The returned format does not contain leading or
 * trailing separator so the separator will need to be added explicitly
 * if more than one call  is required.
 */
EXTERNAL fiftyoneDegreesResultProfileIndex
fiftyoneDegreesIpiGetNetworkIdFromResults(
	fiftyoneDegreesResultsIpi* results,
	char* destination,
	size_t size,
	fiftyoneDegreesResultProfileIndex resultProfileIndex,
	fiftyoneDegreesException* exception);

/**
 * Iterates over the profiles in the data set calling the callback method for
 * any profiles that contain the property and value provided.
 * This currently not applicable for properties 'IpRangeStart', 'IpRangeEnd',
 * 'AverageLocation', 'LocationBoundSouthEast', 'LocationBoundNortWest' as
 * these data are not stored in the profiles collection.
 * @param manager the resource manager containing a IP Intelligence data set initialised
 * by one of the IP Intelligence data set init methods
 * @param propertyName name of the property which the value relates to
 * @param valueName name of the property value which the profiles must contain
 * @param state pointer passed to the callback method
 * @param callback method called when a matching profile is found
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the number of matching profiles iterated over
 */
EXTERNAL uint32_t fiftyoneDegreesIpiIterateProfilesForPropertyAndValue(
	fiftyoneDegreesResourceManager* manager,
	const char* propertyName,
	const char* valueName,
	void* state,
	fiftyoneDegreesProfileIterateMethod callback,
	fiftyoneDegreesException* exception);

/**
 * Get the ipaddress string from the collection item. This should
 * be used on the item returned from #fiftyoneDegreesResultsIpiGetValues
 * where the property is 'RangeStart', 'RangeEnd'.
 * @param item the collection item pointing to the strings item in
 * strings collection
 * @param type the verion of IP which can be check from result
 * @param buffer the preallocated buffer to hold the returned string
 * @param bufferLength the number of bytes allocated for the buffer
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the number of characters added to the buffer
 */
EXTERNAL size_t fiftyoneDegreesIpiGetIpAddressAsString(
	const fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesIpType type,
	char *buffer,
	uint32_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * @name Metadata
 *
 * The properties of the data file, their values, its components and its
 * profiles, with the answers the C++ metadata classes give. Each function
 * takes a data set from #fiftyoneDegreesDataSetIpiGet, which the caller
 * holds while it reads so that every answer comes from the same data file. A
 * property is found by its index in the data file, which counts every
 * property the file defines, including those the data set was not created
 * with. A function that gives text writes it into the buffer provided when it
 * fits, and otherwise into memory the caller frees with fiftyoneDegreesFree
 * when the pointer returned is not the buffer, as
 * #fiftyoneDegreesStoredBinaryValueToText describes.
 * @{
 */

/**
 * Gets the number of properties the data file defines.
 * @param dataSet the data set to read
 * @return the number of properties
 */
EXTERNAL uint32_t fiftyoneDegreesIpiGetPropertyCount(
	fiftyoneDegreesDataSetIpi *dataSet);

/**
 * Gets the index of the property with the name provided, which must match
 * exactly.
 * @param dataSet the data set to read
 * @param propertyName name of the property to find
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the index of the property, or -1 when the data file has no
 * property of that name or a property cannot be read
 */
EXTERNAL int fiftyoneDegreesIpiGetPropertyIndex(
	fiftyoneDegreesDataSetIpi *dataSet,
	const char *propertyName,
	fiftyoneDegreesException *exception);

/**
 * Gets the fields of the property that are not text. Any of the output
 * pointers can be NULL, and the property's name is read only when
 * `isAvailable` or `requiredPropertyIndex` is not.
 * @param dataSet the data set to read
 * @param propertyIndex index of the property
 * @param valueType set to the #fiftyoneDegreesPropertyValueType of the
 * property's values
 * @param componentId set to the id of the component the property belongs to
 * @param displayOrder set to the order to display the property in
 * @param isMandatory set to true when the property is mandatory
 * @param isList set to true when the property can have more than one value
 * @param isObsolete set to true when the property is obsolete
 * @param show set to true when the property should be shown
 * @param showValues set to true when the property's values should be shown
 * @param mapCount set to the number of maps the property is in, which
 * #fiftyoneDegreesIpiGetPropertyMapText names
 * @param isAvailable set to true when the data set was created with the
 * property, so that results can hold its values
 * @param requiredPropertyIndex set to the index the results functions take
 * for the property, or -1 when it has none
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return true when the fields were read, otherwise false. False with no
 * exception set means the properties collection gave nothing at the index.
 */
EXTERNAL bool fiftyoneDegreesIpiGetProperty(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint8_t *valueType,
	uint8_t *componentId,
	uint8_t *displayOrder,
	bool *isMandatory,
	bool *isList,
	bool *isObsolete,
	bool *show,
	bool *showValues,
	uint32_t *mapCount,
	bool *isAvailable,
	int *requiredPropertyIndex,
	fiftyoneDegreesException *exception);

/**
 * Gets where the property's values are in the values collection, and which
 * of them is its default. Any of the output pointers can be NULL, and the
 * default value is found only when `defaultValueIndex` is not.
 * @param dataSet the data set to read
 * @param propertyIndex index of the property
 * @param firstValueIndex set to the index of the property's first value
 * @param valueCount set to the number of values the property has, as
 * #fiftyoneDegreesPropertyGetValueCount counts them
 * @param defaultValueIndex set to the index of the property's default value
 * as #fiftyoneDegreesValueGetDefaultIndex finds it, or -1 when it has none
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return true when the indexes were read, otherwise false. False with no
 * exception set means the properties collection gave nothing at the index.
 */
EXTERNAL bool fiftyoneDegreesIpiGetPropertyValueIndexes(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint32_t *firstValueIndex,
	uint32_t *valueCount,
	long *defaultValueIndex,
	fiftyoneDegreesException *exception);

/**
 * Gets a text of the property, as #fiftyoneDegreesPropertyGetText gives it.
 * The default value is written as the type the property is stored as.
 * @param dataSet the data set to read
 * @param propertyIndex index of the property
 * @param text which text of the property to get
 * @param buffer memory the text is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the null terminated text, or NULL when the property or the text
 * cannot be read or written
 */
EXTERNAL char* fiftyoneDegreesIpiGetPropertyText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	fiftyoneDegreesPropertyText text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the name of one of the maps the property is in, as
 * #fiftyoneDegreesPropertyGetMapText gives it.
 * @param dataSet the data set to read
 * @param propertyIndex index of the property
 * @param mapIndex the place of the map among the property's maps, less than
 * the `mapCount` #fiftyoneDegreesIpiGetProperty gives
 * @param buffer memory the name is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the null terminated name, or NULL. NULL with no exception set means
 * the strings collection gives nothing at the map's offset, and the metadata
 * then leaves the map out.
 */
EXTERNAL char* fiftyoneDegreesIpiGetPropertyMapText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	uint32_t mapIndex,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the index of the property the value belongs to and each of the
 * value's texts, from one read of the value. The method is called with the
 * name, the description and then the URL, each as
 * #fiftyoneDegreesIpiGetValueText gives it.
 * @param dataSet the data set to read
 * @param valueIndex index of the value in the values collection
 * @param propertyIndex set to the index of the value's property, or NULL
 * @param state pointer passed to the method
 * @param method called with each text of the value, or NULL when only the
 * property index is wanted
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return true when the value was read and each text given, otherwise
 * false. False with no exception set means the values collection gave
 * nothing at the index.
 */
EXTERNAL bool fiftyoneDegreesIpiGetValue(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	uint32_t *propertyIndex,
	void *state,
	fiftyoneDegreesValueTextMethod method,
	fiftyoneDegreesException *exception);

/**
 * Gets the index of the property's value with the name provided. Each value
 * is compared with the name as the type the property is stored as, so a
 * number is found when the name has fewer decimal places than the value
 * stored. A property's values are in order, so a binary search finds the
 * value without reading the others.
 * @param dataSet the data set to read
 * @param propertyIndex index of the property
 * @param valueName name of the value to find
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the index of the value in the values collection, or -1 when the
 * property has no value of that name or a value cannot be read
 */
EXTERNAL long fiftyoneDegreesIpiGetValueIndex(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t propertyIndex,
	const char *valueName,
	fiftyoneDegreesException *exception);

/**
 * Gets a text of the value as the metadata gives it. The name and URL are as
 * #fiftyoneDegreesValueGetMetaDataText gives them, the name written as the
 * type the value's property is stored as with a number written to
 * #FIFTYONE_DEGREES_WKT_DECIMAL_PLACES places. The description is always
 * empty.
 * @param dataSet the data set to read
 * @param valueIndex index of the value in the values collection
 * @param text which text of the value to get
 * @param buffer memory the text is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the null terminated text, or NULL when the value or the text
 * cannot be read or written
 */
EXTERNAL char* fiftyoneDegreesIpiGetValueText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t valueIndex,
	fiftyoneDegreesValueText text,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the number of components the data file defines.
 * @param dataSet the data set to read
 * @return the number of components
 */
EXTERNAL uint32_t fiftyoneDegreesIpiGetComponentCount(
	fiftyoneDegreesDataSetIpi *dataSet);

/**
 * Gets the id of the component and the offset of its default profile. An IP
 * Intelligence data file holds no profile ids, so the metadata gives the
 * offset as the default profile's key, and UINT32_MAX for a component with
 * no default profile. Either output pointer can be NULL.
 * @param dataSet the data set to read
 * @param componentIndex index of the component, less than
 * #fiftyoneDegreesIpiGetComponentCount
 * @param componentId set to the id of the component
 * @param defaultProfileOffset set to the offset of the component's default
 * profile, which #fiftyoneDegreesIpiGetProfileByOffset takes
 * @return true when the index is less than the number of components,
 * otherwise false
 */
EXTERNAL bool fiftyoneDegreesIpiGetComponent(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t componentIndex,
	uint8_t *componentId,
	uint32_t *defaultProfileOffset);

/**
 * Gets the name of the component, such as "Location". A name the strings
 * collection gives nothing for is an empty text.
 * @param dataSet the data set to read
 * @param componentIndex index of the component
 * @param buffer memory the name is written into when it fits
 * @param bufferLength the bytes `buffer` holds
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return the null terminated name, or NULL when the index is not less than
 * the number of components or the name cannot be read or written
 */
EXTERNAL char* fiftyoneDegreesIpiGetComponentName(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t componentIndex,
	char *buffer,
	size_t bufferLength,
	fiftyoneDegreesException *exception);

/**
 * Gets the number of profiles the data file holds.
 * @param dataSet the data set to read
 * @return the number of profiles
 */
EXTERNAL uint32_t fiftyoneDegreesIpiGetProfileCount(
	fiftyoneDegreesDataSetIpi *dataSet);

/**
 * Gets the offset of the profile at the index among the data file's
 * profiles, which the metadata gives as the profile's key, and the id of
 * the component it belongs to, as #fiftyoneDegreesIpiGetProfileByOffset
 * gives it. Either output pointer can be NULL.
 * @param dataSet the data set to read
 * @param profileIndex index of the profile, less than
 * #fiftyoneDegreesIpiGetProfileCount
 * @param profileOffset set to the offset of the profile
 * @param componentId set to the id of the profile's component
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return true when the profile was read, otherwise false. False with no
 * exception set means the offset is UINT32_MAX or the profiles collection
 * gave nothing at it.
 */
EXTERNAL bool fiftyoneDegreesIpiGetProfile(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t profileIndex,
	uint32_t *profileOffset,
	uint8_t *componentId,
	fiftyoneDegreesException *exception);

/**
 * Gets the id of the component the profile at the offset provided belongs
 * to.
 * @param dataSet the data set to read
 * @param profileOffset offset of the profile in the profiles collection
 * @param componentId set to the id of the profile's component, or NULL
 * @param exception pointer to an exception data structure to be used if an
 * exception occurs. See exceptions.h
 * @return true when the profile was read, otherwise false. False with no
 * exception set means the offset is UINT32_MAX, which a component with no
 * default profile gives, or the profiles collection gave nothing at it.
 */
EXTERNAL bool fiftyoneDegreesIpiGetProfileByOffset(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t profileOffset,
	uint8_t *componentId,
	fiftyoneDegreesException *exception);

/**
 * @}
 */

/**
 * @}
 */

#endif
