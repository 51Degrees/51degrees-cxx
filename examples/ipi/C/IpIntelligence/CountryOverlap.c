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

/**
@example IpIntelligence/CountryOverlap.c
Country overlap analysis using 51Degrees IP intelligence.

Where IP addresses are used to restrict content to a country, sometimes
called geo fencing, an IP address whose likely area spans an international
border cannot be placed in a single country. This example measures how
often that happens by checking every IPv4 address in the data file, and
names the countries that share each country's address space. The results
can be reproduced with any enterprise IP Intelligence data file.

## What the example produces

A single CSV file and a console summary. Each CSV row pairs the most
probable, or primary, country with a secondary country for a location
confidence and connection type:

- PrimaryCountryCode, PrimaryCountry: ISO code and name of the most
  probable country for the addresses counted by the row.
- SecondaryCountryCode, SecondaryCountry: when these equal the primary
  country the row counts the addresses that resolve entirely to the
  primary country. Otherwise they name another country in the
  population weighted country list, and the row counts the addresses
  that can resolve to both the primary and the secondary country.
  Secondary countries whose share of the weighting is at or below the
  minimum percentage (default 2%) are ignored, including when deciding
  whether an address resolves entirely to the primary country.
- LocationConfidence, ConnectionType: the values of the properties with
  these names. See the
  [property dictionary](https://51degrees.com/developers/property-dictionary?utm_source=code&utm_medium=example&utm_campaign=ip-intelligence-cxx&utm_content=examples-c-ipintelligence-countryoverlap.c&utm_term=property-dictionary)
  for their definitions.
- IpAddressCount: IPv4 addresses counted by the row.
- ProportionOfPrimaryCountryAddresses: IpAddressCount as a proportion,
  where 1 is 100%, of all the addresses whose primary country is the
  row's primary country, across every location confidence and
  connection type.

An address whose area overlaps several countries appears in one row for
each secondary country, so the proportions for a primary country do not
sum to 1.

## Which weighted country list is used

The data provides two weighted country lists for the area associated
with an address. CountryCodesGeographical weights each country by its
share of the area, and CountryCodesPopulation weights each country by
its share of the people living within the area. This example uses the
population list, because the decisions it informs, such as whether a
viewer is inside a licensed territory, are about people rather than
land. A largely empty border region contributes little to a licensing
decision even when it covers a lot of ground. The area weighted split
is reported in the console summary for comparison, and probe mode
prints both lists for a single address.

## How it works

Every IPv4 address is checked individually because location results
change within network ranges. For each address the component graph is
evaluated directly, which takes a few microseconds. The full weighted
property values are only resolved through the standard results API when
the graph result changes from one address to the next, and resolutions
are cached by graph offset so each distinct location is resolved once.
A full sweep of the 4.3 billion IPv4 addresses completes in around an
hour on a modern multi core machine.

After the sweep, randomly selected addresses are compared against the
normal single address lookup process and the outcome of each comparison
is printed, so the method can be seen to produce the same values as
individual lookups would.

## Requirements

The example requires an enterprise IP Intelligence data file that
includes the weighted country code properties. It stops with a clear
message, and a distinct exit code, when the data file does not include
them, which is the case for the Lite data file. To obtain an enterprise
data file see our
[pricing](https://51degrees.com/pricing?utm_source=code&utm_medium=example&utm_campaign=ip-intelligence-cxx&utm_content=examples-c-ipintelligence-countryoverlap.c&utm_term=enterprise-data-file).

Only IPv4 is swept. IPv6 addresses are too numerous to check one by one,
although individual IPv6 addresses can be checked with the probe option.

@include{doc} example-require-datafile-ipi.txt

## Usage

```
CountryOverlap <data file> [output csv] [threads] [chunks] [min percent]
CountryOverlap <data file> --probe <ip> [ip...]
```

- data file: path to an enterprise .ipi data file.
- output csv: path for the CSV, default country-overlap.csv.
- threads: number of worker threads, default 10. Each thread holds its
  own cache of resolved locations of about 600 MB, and a country pair
  block of about 3.5 MB for each combination of location confidence and
  connection type it meets. Merging the results allocates one further
  set of pair blocks. Ten threads therefore need something over 7 GB in
  addition to the memory used by the data file, so choose fewer threads
  on machines with less memory. When a cache cannot be allocated at full
  size a smaller one is used, which is slower but produces the same
  results.
- chunks: number of /8 chunks of the IPv4 space to process starting at
  0.0.0.0, default 256 which is the whole space. Small values are useful
  for testing.
- min percent: secondary countries whose share of the population
  weighting is this percentage or lower are ignored, default 2. The
  areas can include countries with negligible weightings where the area
  shape extends beyond the evidence, so the default focuses the output
  on meaningful overlap. Set to 0 to include every listed country.
- --probe: print the resolved values for the IPv4 or IPv6 addresses
  provided so they can be compared against other 51Degrees APIs. At
  least one address is required.
*/

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../../src/ipi.h"
#include "../../../src/fiftyone.h"
#include "../../../src/ipi_weighted_results.h"

/** Properties the analysis needs. */
#define PROPERTY_COUNTRY_CODE "CountryCode"
#define PROPERTY_COUNTRY "Country"
#define PROPERTY_CONFIDENCE "LocationConfidence"
#define PROPERTY_CONNECTION "ConnectionType"
#define PROPERTY_GEO "CountryCodesGeographical"
#define PROPERTY_POP "CountryCodesPopulation"
static const char* countryOverlapProperties =
	PROPERTY_COUNTRY_CODE ","
	PROPERTY_COUNTRY ","
	PROPERTY_CONFIDENCE ","
	PROPERTY_CONNECTION ","
	PROPERTY_GEO ","
	PROPERTY_POP;

/** Return codes for fiftyoneDegreesIpiCountryOverlap. */
#define COUNTRY_OVERLAP_OK 0
#define COUNTRY_OVERLAP_FAILED 1
#define COUNTRY_OVERLAP_PROPERTIES_MISSING 2

/** Country slots: 26 x 26 two letter codes plus Unknown and Failed. */
#define COUNTRY_SLOTS (26 * 26 + 2)
#define COUNTRY_UNKNOWN (26 * 26)
#define COUNTRY_FAILED (26 * 26 + 1)

/** Maximum number of distinct values for the confidence and connection
type dimensions. */
#define VALUE_SLOTS 16

/** Maximum number of component graphs the required properties span. */
#define MAX_COMPONENTS 4

/** Maximum number of entries in a weighted list that are compared when
counting distinct countries. */
#define MAX_LIST_VALUES 64

/** Number of shared countries remembered for each distinct location,
kept in descending weighting order. Matches the maximum number of list
values compared, so every qualifying country can be stored. Lists with
more distinct countries than this are flagged and counted so truncation
is visible in the output. */
#define SHARED_STORE MAX_LIST_VALUES

/** Default minimum share of the population weighting a secondary
country must exceed to be counted. The areas can include countries with
negligible weightings where the area shape extends beyond the evidence,
so secondary countries at or below this percentage are ignored by
default. Set to zero to include every listed country. */
#define DEFAULT_MIN_SECONDARY_PERCENT 2.0

/** Number of address bits covered by a chunk, the unit of work claimed
by a thread. The default of 24 makes each chunk a /8. The tests define
a smaller value before including this file so that they can sweep a
small part of the address space in seconds. */
#ifndef COUNTRY_OVERLAP_CHUNK_BITS
#define COUNTRY_OVERLAP_CHUNK_BITS 24
#endif
#define CHUNK_SIZE ((uint64_t)1 << COUNTRY_OVERLAP_CHUNK_BITS)
#define MAX_CHUNKS (1 << (32 - COUNTRY_OVERLAP_CHUNK_BITS))

/** Addresses between progress counter updates, and the mask that spots
them. Measured from the start of a chunk rather than from the start of
the address space, so the total a chunk reports is exactly its size
whatever the chunk size. */
#define PROGRESS_INTERVAL ((uint64_t)1 << 20)
#define PROGRESS_MASK (PROGRESS_INTERVAL - 1)

/** Addresses between checks for an early stop request. Shorter than the
progress interval so that a chunk smaller than that interval, which the
tests use, still notices a stop request part way through. */
#define STOP_CHECK_MASK (((uint64_t)1 << 16) - 1)

/** Number of bits in the size of each thread's cache of resolved graph
offsets. The default of 22 gives 4,194,304 entries, about 600 MB per
thread, which keeps the cache well below full for the distinct
locations a thread meets in a full sweep. The tests define a smaller
value before including this file because they sweep far fewer
addresses. */
#ifndef COUNTRY_OVERLAP_CACHE_BITS
#define COUNTRY_OVERLAP_CACHE_BITS 22
#endif
#define CACHE_SIZE (1u << COUNTRY_OVERLAP_CACHE_BITS)

/** Smallest cache tried when the full size cannot be allocated. Below
this almost every resolution misses the cache and the sweep slows to
the point where reporting an error is more useful. */
#define MIN_CACHE_BITS 12
#if COUNTRY_OVERLAP_CACHE_BITS < MIN_CACHE_BITS
#error COUNTRY_OVERLAP_CACHE_BITS must be at least MIN_CACHE_BITS
#endif

/** Number of random addresses compared against the normal lookup process
after the sweep to validate the graph based method. */
#define SPOT_CHECKS 20

/** Resolved flag: at least one secondary country qualifies in the
population weighted list. */
#define RESOLVED_MULTI_COUNTRY 1
/** Resolved flag: at least one secondary country qualifies in the area
weighted list. Reported for comparison only. */
#define RESOLVED_MULTI_COUNTRY_AREA 2
/** Resolved flag: the population weighted list had more distinct
countries than MAX_LIST_VALUES, so the lowest weighted were dropped. */
#define RESOLVED_TRUNCATED 4

/**
 * Graph offsets for every component the required properties belong to.
 * Values are only reused between addresses when all the offsets match,
 * so every component that any property read by the analysis depends on
 * must be included. Unused entries are zero so keys compare with memcmp.
 */
typedef struct country_overlap_segment_key_t {
	uint32_t offsets[MAX_COMPONENTS];
} SegmentKey;

/** Resolved values for a combination of graph offsets. */
typedef struct country_overlap_resolved_t {
	SegmentKey key; /**< Graph offsets the values were resolved for */
	bool occupied; /**< True when the cache entry holds values */
	uint16_t countryIndex; /**< Index into the country dimension */
	uint8_t confidenceIndex; /**< Index into the confidence dimension */
	uint8_t connectionIndex; /**< Index into the connection dimension */
	uint8_t flags; /**< Combination of the RESOLVED_ flags */
	uint8_t sharedCount; /**< Number of entries in shared */
	uint16_t shared[SHARED_STORE]; /**< Country indexes that share the
								   area, highest weighted first */
} Resolved;

/** A cell of the output matrix. */
typedef struct country_overlap_cell_t {
	uint64_t segments; /**< Number of contiguous segments of addresses */
	uint64_t addresses;
	uint64_t single; /**< Addresses whose people are all in the primary
					 country */
	uint64_t multi; /**< Addresses that also relate to other countries */
	uint64_t multiArea; /**< Addresses that also relate to other countries
						by share of the area rather than of the people,
						reported in the summary for comparison */
} Cell;

/** Cells for all the combinations of the three dimensions. */
typedef Cell Matrix[COUNTRY_SLOTS][VALUE_SLOTS][VALUE_SLOTS];

/** Longest value name held in a value table. The location confidence and
connection type values are short words, so the names are copied into the
table's own storage. Allocating them instead meant a failed allocation
could leave a null in a table that every reader prints. */
#define VALUE_NAME_MAX 128

/** Value names for a dimension, built dynamically from the data. */
typedef struct country_overlap_value_table_t {
	char names[VALUE_SLOTS][VALUE_NAME_MAX];
	int count;
} ValueTable;

/**
 * Addresses that can resolve to both a primary and a secondary country
 * for each combination of confidence and connection type. Each block is
 * a COUNTRY_SLOTS by COUNTRY_SLOTS matrix indexed
 * [primary * COUNTRY_SLOTS + secondary], allocated on first use because
 * only a few of the confidence and connection combinations occur in
 * real data.
 */
typedef struct country_overlap_shared_blocks_t {
	uint64_t* blocks[VALUE_SLOTS][VALUE_SLOTS];
} SharedBlocks;

/**
 * Returns the pair count block for the confidence and connection type,
 * allocating a zeroed block on first use. Returns null when the
 * allocation fails. Callers must count the failure, because a missing
 * block removes every secondary country row for the combination from the
 * CSV, which is incomplete output rather than a different result.
 */
static uint64_t* sharedBlockGet(SharedBlocks* sharedBlocks, int f, int n) {
	uint64_t* block = sharedBlocks->blocks[f][n];
	if (block == NULL) {
		const size_t size =
			sizeof(uint64_t) * COUNTRY_SLOTS * COUNTRY_SLOTS;
		block = (uint64_t*)Malloc(size);
		if (block != NULL) {
			memset(block, 0, size);
			sharedBlocks->blocks[f][n] = block;
		}
	}
	return block;
}

static void sharedBlocksFree(SharedBlocks* sharedBlocks) {
	for (int f = 0; f < VALUE_SLOTS; f++) {
		for (int n = 0; n < VALUE_SLOTS; n++) {
			if (sharedBlocks->blocks[f][n] != NULL) {
				Free(sharedBlocks->blocks[f][n]);
				sharedBlocks->blocks[f][n] = NULL;
			}
		}
	}
}

/** State shared by all threads. */
typedef struct country_overlap_shared_state_t {
	ResourceManager* manager;
	DataSetIpi* dataSet;
	byte componentIds[MAX_COMPONENTS]; /**< Distinct component ids of the
									   required properties */
	int componentCount;
	int reqIndexCountryCode;
	int reqIndexCountry;
	int reqIndexConfidence;
	int reqIndexConnection;
	int reqIndexGeo;
	int reqIndexPop;
	int endChunk; /**< Chunk after the last one to process */
	double minShare; /**< Minimum share of the weighting, as a fraction
					 of 1, a secondary country must exceed to count */
	volatile long nextChunk; /**< Next chunk to be claimed by a thread */
	volatile long threadsDone;
	volatile long stopRequested; /**< Non zero when the workers must stop
								 early, because not every worker could
								 be started */
} SharedState;

/** State private to a single worker thread. */
typedef struct country_overlap_thread_state_t {
	SharedState* shared;
	ResultsIpi* results;
	Resolved* cache;
	uint32_t cacheMask; /**< Entries in cache minus one, a power of two
						less one so it can mask a hash into a slot */
	Matrix cells;
	SharedBlocks sharedBlocks;
	char* countryNames[COUNTRY_SLOTS];
	ValueTable confidence;
	ValueTable connection;
	volatile uint64_t addressesDone;
	uint64_t evaluations;
	uint64_t resolutions;
	uint64_t failures;
	uint64_t truncations; /**< Distinct locations whose population list
						  had more distinct countries than SHARED_STORE */
	uint64_t pairBlockFailures; /**< Segments whose pair counts were lost
								because a block could not be allocated */
} ThreadState;

static void countryOverlapReportStatus(
	StatusCode status,
	const char* fileName) {
	const char* message = StatusGetMessage(status, fileName);
	printf("%s\n", message);
	Free((void*)message);
}

/**
 * Returns the status code carried by the exception, or NOT_SET when the
 * exception holds no failure. Wrapped in a function because the exception
 * type is a void pointer when FIFTYONE_DEGREES_EXCEPTIONS_DISABLED is
 * defined, and both arms of a conditional expression are compiled whatever
 * the condition, so reading the status member inline broke the build for
 * that supported option.
 */
static StatusCode countryOverlapExceptionStatus(Exception* exception) {
#ifdef FIFTYONE_DEGREES_EXCEPTIONS_DISABLED
	(void)exception;
	return NOT_SET;
#else
	return EXCEPTION_FAILED ? exception->status : NOT_SET;
#endif
}

static char* countryOverlapDuplicate(const char* value) {
	size_t length = strlen(value) + 1;
	char* copy = (char*)Malloc(length);
	if (copy != NULL) {
		memcpy(copy, value, length);
	}
	return copy;
}

/**
 * Returns the index in the value table for the name, adding the name if it
 * has not been seen before. The last slot is used when the table is full
 * which is not expected with real data.
 */
static int valueIndex(ValueTable* table, const char* name) {
	for (int i = 0; i < table->count; i++) {
		if (strcmp(table->names[i], name) == 0) {
			return i;
		}
	}
	if (table->count < VALUE_SLOTS) {
		snprintf(
			table->names[table->count],
			VALUE_NAME_MAX,
			"%s",
			name);
		return table->count++;
	}
	return VALUE_SLOTS - 1;
}

/**
 * Returns the country slot for a two letter ISO country code, or the
 * unknown slot when the code is not two upper case letters.
 */
static int countryIndex(const char* code) {
	if (code != NULL &&
		code[0] >= 'A' && code[0] <= 'Z' &&
		code[1] >= 'A' && code[1] <= 'Z' &&
		code[2] == '\0') {
		return (code[0] - 'A') * 26 + (code[1] - 'A');
	}
	return COUNTRY_UNKNOWN;
}

/** Returns the two letter code for a country slot into the buffer. */
static const char* countryCode(int index, char buffer[8]) {
	if (index == COUNTRY_UNKNOWN) {
		return "Unknown";
	}
	if (index == COUNTRY_FAILED) {
		return "Failed";
	}
	buffer[0] = (char)('A' + index / 26);
	buffer[1] = (char)('A' + index % 26);
	buffer[2] = '\0';
	return buffer;
}

/** Summed weighting of each distinct country in a weighted list. */
typedef struct country_overlap_country_weights_t {
	uint16_t indexes[MAX_LIST_VALUES]; /**< Country slot of each entry */
	double weights[MAX_LIST_VALUES]; /**< Summed weighting of each entry */
	int count; /**< Number of distinct countries held */
	double total; /**< Total weighting of the whole list, including any
				  countries that did not fit */
	bool truncated; /**< True when the list had more distinct countries
					than MAX_LIST_VALUES */
} CountryWeights;

/**
 * Sums the weighting for each distinct country in the list for the
 * required property index. A country can appear more than once in a
 * list, so the entries are summed before any comparison with the
 * minimum share. The single place the lists are read, so the population
 * and area classifications always apply the same rules.
 */
static void sumCountryWeights(
	CountryWeights* result,
	const WeightedValuesCollection* collection,
	int requiredPropertyIndex) {
	memset(result, 0, sizeof(CountryWeights));
	for (uint32_t i = 0; i < collection->itemsCount; i++) {
		const fiftyoneDegreesWeightedValueHeader* header =
			collection->items[i];
		if (header->requiredPropertyIndex != requiredPropertyIndex ||
			header->valueType !=
			FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING) {
			continue;
		}
		const double weight = (double)header->rawWeighting;
		result->total += weight;
		const uint16_t index = (uint16_t)countryIndex(
			((const fiftyoneDegreesWeightedString*)header)->value);
		bool found = false;
		for (int s = 0; s < result->count && found == false; s++) {
			if (result->indexes[s] == index) {
				result->weights[s] += weight;
				found = true;
			}
		}
		if (found == false) {
			if (result->count < MAX_LIST_VALUES) {
				result->indexes[result->count] = index;
				result->weights[result->count] = weight;
				result->count++;
			}
			else {
				// Recorded so the output can say the pair counts for
				// this location exclude its lowest weighted countries.
				result->truncated = true;
			}
		}
	}
}

/**
 * Returns true when the entry is a secondary country that counts towards
 * the overlap. The primary country and entries that are not a valid
 * country code are never secondary, and neither are countries whose
 * share of the total weighting does not exceed minShare.
 */
static bool isQualifyingSecondary(
	const CountryWeights* weights,
	int entry,
	uint16_t primaryIndex,
	double minShare) {
	return weights->indexes[entry] != primaryIndex &&
		weights->indexes[entry] != COUNTRY_UNKNOWN &&
		weights->weights[entry] > weights->total * minShare;
}

/**
 * Returns the string value with the highest weighting for the required
 * property index, or null when the property has no values.
 */
static const char* highestWeighted(
	const WeightedValuesCollection* collection,
	int requiredPropertyIndex) {
	const char* best = NULL;
	uint32_t bestWeight = 0;
	for (uint32_t i = 0; i < collection->itemsCount; i++) {
		const fiftyoneDegreesWeightedValueHeader* header =
			collection->items[i];
		if (header->requiredPropertyIndex != requiredPropertyIndex ||
			header->valueType !=
			FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING) {
			continue;
		}
		if (best == NULL || header->rawWeighting > bestWeight) {
			best = ((const fiftyoneDegreesWeightedString*)header)->value;
			bestWeight = header->rawWeighting;
		}
	}
	return best;
}

/**
 * Fills the shared country indexes of the resolved values with the
 * qualifying secondary countries in the weighted list, ordered with the
 * highest weighted first. Countries whose share of the list's total
 * weighting does not exceed minShare are ignored, because the areas can
 * include countries with negligible weightings where the area shape
 * extends beyond the evidence. SHARED_STORE equals MAX_LIST_VALUES so
 * every qualifying country held in the weights fits.
 */
static void setSharedCountries(
	Resolved* resolved,
	const CountryWeights* weights,
	double minShare) {
	bool used[MAX_LIST_VALUES];
	memset(used, 0, sizeof(used));
	int stored = 0;
	for (;;) {
		int best = -1;
		double bestWeight = 0;
		for (int s = 0; s < weights->count; s++) {
			if (used[s] == false &&
				isQualifyingSecondary(
					weights,
					s,
					resolved->countryIndex,
					minShare) == true &&
				(best < 0 || weights->weights[s] > bestWeight)) {
				best = s;
				bestWeight = weights->weights[s];
			}
		}
		if (best < 0) {
			break;
		}
		used[best] = true;
		resolved->shared[stored++] = weights->indexes[best];
	}
	resolved->sharedCount = (uint8_t)stored;
}

/**
 * Returns true when at least one secondary country in the weights
 * qualifies, applying the same rules as setSharedCountries.
 */
static bool hasQualifyingSecondary(
	const CountryWeights* weights,
	uint16_t primaryIndex,
	double minShare) {
	for (int s = 0; s < weights->count; s++) {
		if (isQualifyingSecondary(
			weights,
			s,
			primaryIndex,
			minShare) == true) {
			return true;
		}
	}
	return false;
}

/**
 * Resolves the values for the address using the normal lookup process.
 * The full lookup is only used once per distinct combination of graph
 * offsets because the hot loop detects repeats with the much cheaper
 * graph evaluations.
 */
static Resolved resolveAddress(ThreadState* state, uint32_t address) {
	SharedState* shared = state->shared;
	Resolved resolved;
	memset(&resolved, 0, sizeof(resolved));
	resolved.countryIndex = COUNTRY_FAILED;
	resolved.confidenceIndex = (uint8_t)valueIndex(
		&state->confidence,
		"Failed");
	resolved.connectionIndex = (uint8_t)valueIndex(
		&state->connection,
		"Failed");
	EXCEPTION_CREATE;

	byte bytes[4];
	bytes[0] = (byte)(address >> 24);
	bytes[1] = (byte)(address >> 16);
	bytes[2] = (byte)(address >> 8);
	bytes[3] = (byte)address;
	ResultsIpiFromIpAddress(
		state->results,
		bytes,
		sizeof(bytes),
		FIFTYONE_DEGREES_IP_TYPE_IPV4,
		exception);
	if (EXCEPTION_FAILED) {
		state->failures++;
		return resolved;
	}

	WeightedValuesCollection collection = ResultsIpiGetValuesCollection(
		state->results,
		NULL,
		0,
		NULL,
		exception);
	if (EXCEPTION_FAILED) {
		state->failures++;
		return resolved;
	}

	const char* code = highestWeighted(
		&collection,
		shared->reqIndexCountryCode);
	const char* confidence = highestWeighted(
		&collection,
		shared->reqIndexConfidence);
	const char* connection = highestWeighted(
		&collection,
		shared->reqIndexConnection);
	resolved.countryIndex = (uint16_t)countryIndex(code);
	resolved.confidenceIndex = (uint8_t)valueIndex(
		&state->confidence,
		confidence != NULL ? confidence : "Unknown");
	resolved.connectionIndex = (uint8_t)valueIndex(
		&state->connection,
		connection != NULL ? connection : "Unknown");
	// The population weighted list is used because the decisions this
	// analysis informs, such as whether a viewer is inside a licensed
	// territory, are about people rather than land. A country's share
	// of the people living within the area is therefore the meaningful
	// measure, not its share of the area itself.
	CountryWeights weights;
	sumCountryWeights(&weights, &collection, shared->reqIndexPop);
	setSharedCountries(&resolved, &weights, shared->minShare);
	// The address is multi country when at least one qualifying
	// secondary country exists, so the classification always agrees
	// with the pair rows in the CSV.
	if (resolved.sharedCount > 0) {
		resolved.flags |= RESOLVED_MULTI_COUNTRY;
	}
	if (weights.truncated == true) {
		resolved.flags |= RESOLVED_TRUNCATED;
		state->truncations++;
	}
	// The area weighted list is classified with the same rules so the
	// two figures in the summary are directly comparable.
	sumCountryWeights(&weights, &collection, shared->reqIndexGeo);
	if (hasQualifyingSecondary(
		&weights,
		resolved.countryIndex,
		shared->minShare) == true) {
		resolved.flags |= RESOLVED_MULTI_COUNTRY_AREA;
	}

	// Capture the display name for the country the first time the
	// country slot is seen by this thread.
	if (resolved.countryIndex < COUNTRY_UNKNOWN &&
		state->countryNames[resolved.countryIndex] == NULL) {
		const char* name = highestWeighted(
			&collection,
			shared->reqIndexCountry);
		if (name != NULL) {
			state->countryNames[resolved.countryIndex] =
				countryOverlapDuplicate(name);
		}
	}

	WeightedValuesCollectionRelease(&collection);
	state->resolutions++;
	return resolved;
}

/**
 * Returns the values used for addresses whose graph evaluation failed,
 * so they are counted in the Failed row rather than lost.
 */
static Resolved failedResolved(ThreadState* state) {
	Resolved failed;
	memset(&failed, 0, sizeof(failed));
	failed.countryIndex = COUNTRY_FAILED;
	failed.confidenceIndex = (uint8_t)valueIndex(
		&state->confidence,
		"Failed");
	failed.connectionIndex = (uint8_t)valueIndex(
		&state->connection,
		"Failed");
	return failed;
}

/** Returns the starting cache slot for the key. */
static uint32_t cacheSlot(
	const ThreadState* state,
	const SegmentKey* key) {
	uint64_t hash = 0;
	for (int c = 0; c < state->shared->componentCount; c++) {
		hash = (hash ^ key->offsets[c]) * 0x9E3779B97F4A7C15ull;
	}
	return (uint32_t)(hash >> 32) & state->cacheMask;
}

/**
 * Returns the resolved values for the combined graph offsets, using the
 * thread's cache so that each distinct combination is resolved only once
 * per thread where cache capacity allows.
 */
static Resolved resolveKey(
	ThreadState* state,
	const SegmentKey* key,
	uint32_t address) {
	uint32_t slot = cacheSlot(state, key);
	for (uint32_t probes = 0; probes < 64; probes++) {
		Resolved* entry = &state->cache[slot];
		if (entry->occupied == true &&
			memcmp(&entry->key, key, sizeof(SegmentKey)) == 0) {
			return *entry;
		}
		if (entry->occupied == false) {
			Resolved resolved = resolveAddress(state, address);
			// A failed resolution can be transient, for example a read
			// error from the data file, so it is not cached. Caching it
			// would count every later segment with the same offsets as
			// failed for the rest of the thread's sweep.
			if (resolved.countryIndex != COUNTRY_FAILED) {
				resolved.key = *key;
				resolved.occupied = true;
				*entry = resolved;
			}
			return resolved;
		}
		slot = (slot + 1) & state->cacheMask;
	}
	// The cache neighbourhood is full. Resolve without caching, which is
	// slower but still correct.
	return resolveAddress(state, address);
}

/** Adds a completed segment of addresses to the matrix. */
static void addSegment(
	ThreadState* state,
	const Resolved* resolved,
	uint64_t addresses) {
	Cell* cell = &state->cells
		[resolved->countryIndex]
		[resolved->confidenceIndex]
		[resolved->connectionIndex];
	cell->segments++;
	cell->addresses += addresses;
	if ((resolved->flags & RESOLVED_MULTI_COUNTRY) != 0) {
		cell->multi += addresses;
	}
	else {
		cell->single += addresses;
	}
	if ((resolved->flags & RESOLVED_MULTI_COUNTRY_AREA) != 0) {
		cell->multiArea += addresses;
	}
	if (resolved->sharedCount > 0) {
		uint64_t* block = sharedBlockGet(
			&state->sharedBlocks,
			resolved->confidenceIndex,
			resolved->connectionIndex);
		if (block != NULL) {
			uint64_t* row =
				block + (size_t)resolved->countryIndex * COUNTRY_SLOTS;
			for (int s = 0; s < resolved->sharedCount; s++) {
				row[resolved->shared[s]] += addresses;
			}
		}
		else {
			// Counted so the run reports the gap and fails, rather than
			// writing a CSV that silently has no secondary country rows
			// for this combination.
			state->pairBlockFailures++;
		}
	}
}

/**
 * Evaluates each of the required component graphs for the address and
 * records the offsets in the key. Returns false when any evaluation
 * fails. Failure is returned separately from the key because every
 * offset value, including the one a graph returns when the address has
 * no match, is a valid result that must not be mistaken for a failure.
 */
static bool evaluateKey(
	ThreadState* state,
	fiftyoneDegreesIpAddress address,
	SegmentKey* key) {
	SharedState* shared = state->shared;
	const fiftyoneDegreesIpiCgArray* graphs =
		shared->dataSet->graphsArray;
	EXCEPTION_CREATE;
	memset(key, 0, sizeof(SegmentKey));
	for (int c = 0; c < shared->componentCount; c++) {
		fiftyoneDegreesIpiCgResult result =
			fiftyoneDegreesIpiGraphEvaluate(
				graphs,
				shared->componentIds[c],
				address,
				exception);
		state->evaluations++;
		if (EXCEPTION_FAILED) {
			EXCEPTION_CLEAR;
			state->failures++;
			return false;
		}
		key->offsets[c] = result.rawOffset;
	}
	return true;
}

/**
 * Adds the segment that starts at the address to the matrix, resolving
 * its values unless the graph evaluation failed.
 */
static void completeSegment(
	ThreadState* state,
	bool evaluated,
	const SegmentKey* key,
	uint64_t segmentStart,
	uint64_t addresses) {
	const Resolved resolved = evaluated == true ?
		resolveKey(state, key, (uint32_t)segmentStart) :
		failedResolved(state);
	addSegment(state, &resolved, addresses);
}

/**
 * Sweeps a single chunk of the IPv4 address space evaluating the
 * component graphs for every address. Full value resolution only happens
 * when the combined graph offsets change from one address to the next.
 */
static void sweepChunk(ThreadState* state, uint32_t chunk) {
	const uint64_t start = (uint64_t)chunk << COUNTRY_OVERLAP_CHUNK_BITS;
	const uint64_t end = start + CHUNK_SIZE - 1;

	fiftyoneDegreesIpAddress address;
	memset(&address, 0, sizeof(address));
	address.type = FIFTYONE_DEGREES_IP_TYPE_IPV4;

	SegmentKey segmentKey = { { 0 } };
	SegmentKey key = { { 0 } };
	bool segmentEvaluated = false;
	bool segmentValid = false;
	uint64_t segmentStart = start;

	for (uint64_t ip = start; ip <= end; ip++) {
		address.value[0] = (byte)(ip >> 24);
		address.value[1] = (byte)(ip >> 16);
		address.value[2] = (byte)(ip >> 8);
		address.value[3] = (byte)ip;

		const bool evaluated = evaluateKey(state, address, &key);

		if (segmentValid == false) {
			segmentKey = key;
			segmentEvaluated = evaluated;
			segmentValid = true;
			segmentStart = ip;
		}
		else if (evaluated != segmentEvaluated ||
			(evaluated == true &&
			memcmp(&key, &segmentKey, sizeof(SegmentKey)) != 0)) {
			completeSegment(
				state,
				segmentEvaluated,
				&segmentKey,
				segmentStart,
				ip - segmentStart);
			segmentKey = key;
			segmentEvaluated = evaluated;
			segmentStart = ip;
		}

		// Stop part way through the chunk when asked, so that a failed
		// start does not wait for a whole chunk to finish, and update the
		// progress counter on a longer interval. Both intervals count from
		// the start of the chunk. Counting from the start of the address
		// space meant a chunk shorter than the progress interval reported
		// a whole interval whenever it happened to contain an aligned
		// address, and its own size again below.
		const uint64_t covered = ip - start + 1;
		if ((covered & STOP_CHECK_MASK) == 0 &&
			state->shared->stopRequested != 0) {
			return;
		}
		if ((covered & PROGRESS_MASK) == 0) {
			state->addressesDone += PROGRESS_INTERVAL;
		}
	}
	// Count the addresses left over when the chunk is not a whole number
	// of progress intervals.
	state->addressesDone += CHUNK_SIZE & PROGRESS_MASK;
	if (segmentValid == true) {
		completeSegment(
			state,
			segmentEvaluated,
			&segmentKey,
			segmentStart,
			end - segmentStart + 1);
	}
}

/**
 * Validates the sweep's method for a single address by comparing the
 * values the sweep recorded for the address's segment against the values
 * the normal lookup process returns for the address itself.
 *
 * The sweep resolves values once at the start of each run of addresses
 * that share the same graph offsets. This check finds the start of the
 * run that contains the address, resolves both the run start and the
 * address with the normal lookup process, and compares the outcome. A
 * match shows the address was counted with the same values a single
 * lookup would have returned for it.
 */
static bool spotCheck(ThreadState* state, uint32_t address) {
	fiftyoneDegreesIpAddress ip;
	memset(&ip, 0, sizeof(ip));
	ip.type = FIFTYONE_DEGREES_IP_TYPE_IPV4;

	// Get the combined graph offsets for the address.
	ip.value[0] = (byte)(address >> 24);
	ip.value[1] = (byte)(address >> 16);
	ip.value[2] = (byte)(address >> 8);
	ip.value[3] = (byte)address;
	SegmentKey key;
	const bool evaluated = evaluateKey(state, ip, &key);

	// Walk back to the start of the run of addresses sharing the
	// offsets, bounded so that very large runs do not slow the check.
	uint32_t start = address;
	for (int steps = 0; steps < 65536 && start > 0; steps++) {
		const uint32_t previous = start - 1;
		ip.value[0] = (byte)(previous >> 24);
		ip.value[1] = (byte)(previous >> 16);
		ip.value[2] = (byte)(previous >> 8);
		ip.value[3] = (byte)previous;
		SegmentKey previousKey;
		const bool previousEvaluated =
			evaluateKey(state, ip, &previousKey);
		if (previousEvaluated != evaluated ||
			(evaluated == true &&
			memcmp(&previousKey, &key, sizeof(SegmentKey)) != 0)) {
			break;
		}
		start = previous;
	}

	// Resolve both addresses with the normal lookup process and compare
	// all the values used by the analysis including the shared country
	// list.
	const Resolved atStart = resolveAddress(state, start);
	const Resolved atAddress = resolveAddress(state, address);
	// Two failed lookups return identical Failed values, so without this
	// check they would report a match when nothing was compared.
	const bool resolved =
		atStart.countryIndex != COUNTRY_FAILED &&
		atAddress.countryIndex != COUNTRY_FAILED;
	bool match =
		resolved == true &&
		atStart.countryIndex == atAddress.countryIndex &&
		atStart.confidenceIndex == atAddress.confidenceIndex &&
		atStart.connectionIndex == atAddress.connectionIndex &&
		atStart.flags == atAddress.flags &&
		atStart.sharedCount == atAddress.sharedCount;
	for (int s = 0; match == true && s < atStart.sharedCount; s++) {
		match = atStart.shared[s] == atAddress.shared[s];
	}
	char bufferA[8];
	char bufferB[8];
	printf(
		"Spot check %u.%u.%u.%u: sweep values from %u.%u.%u.%u "
		"(%s %s %s %s) vs normal lookup (%s %s %s %s) %s\n",
		address >> 24, (address >> 16) & 255,
		(address >> 8) & 255, address & 255,
		start >> 24, (start >> 16) & 255,
		(start >> 8) & 255, start & 255,
		countryCode(atStart.countryIndex, bufferA),
		state->confidence.names[atStart.confidenceIndex],
		state->connection.names[atStart.connectionIndex],
		(atStart.flags & RESOLVED_MULTI_COUNTRY) != 0 ?
		"multi-country" : "single-country",
		countryCode(atAddress.countryIndex, bufferB),
		state->confidence.names[atAddress.confidenceIndex],
		state->connection.names[atAddress.connectionIndex],
		(atAddress.flags & RESOLVED_MULTI_COUNTRY) != 0 ?
		"multi-country" : "single-country",
		resolved == false ? "LOOKUP FAILED" :
		match == true ? "MATCH" : "MISMATCH");
	return match;
}

/** Worker thread claiming chunks until none remain. */
static void sweepThread(void* statePointer) {
	ThreadState* state = (ThreadState*)statePointer;
	SharedState* shared = state->shared;
	for (;;) {
		if (shared->stopRequested != 0) {
			break;
		}
		long chunk =
			FIFTYONE_DEGREES_INTERLOCK_INC(&shared->nextChunk) - 1;
		if (chunk >= shared->endChunk) {
			break;
		}
		sweepChunk(state, (uint32_t)chunk);
	}
	FIFTYONE_DEGREES_INTERLOCK_INC(&shared->threadsDone);
}

/**
 * Starts a worker thread for the state. Returns false when the operating
 * system could not create the thread. The create macro assigns a handle
 * on Windows and returns an error code elsewhere, so each is checked in
 * its own way.
 */
static bool threadStart(FIFTYONE_DEGREES_THREAD* thread, ThreadState* state) {
#ifdef _MSC_VER
	FIFTYONE_DEGREES_THREAD_CREATE(
		*thread,
		(FIFTYONE_DEGREES_THREAD_ROUTINE)&sweepThread,
		state);
	return *thread != NULL;
#else
	return FIFTYONE_DEGREES_THREAD_CREATE(
		*thread,
		(FIFTYONE_DEGREES_THREAD_ROUTINE)&sweepThread,
		state) == 0;
#endif
}

/** Sleeps the calling thread for one second. */
static void sleepOneSecond(void) {
#ifdef _MSC_VER
	Sleep(1000);
#else
	sleep(1);
#endif
}

/**
 * Allocates the thread's cache of resolved graph offsets. When the full
 * size cannot be allocated the size is halved until it can, down to
 * MIN_CACHE_BITS, because a smaller cache only slows the sweep whereas
 * failing to allocate stops it. Returns false when even the smallest
 * cache cannot be allocated.
 */
static bool threadCacheCreate(ThreadState* state) {
	for (uint32_t size = CACHE_SIZE;
		size >= (1u << MIN_CACHE_BITS);
		size >>= 1) {
		state->cache = (Resolved*)Malloc(sizeof(Resolved) * size);
		if (state->cache != NULL) {
			// Every entry must start unoccupied for the cache probe.
			memset(state->cache, 0, sizeof(Resolved) * size);
			state->cacheMask = size - 1;
			if (size < CACHE_SIZE) {
				printf(
					"Using a reduced cache of %u entries for a thread "
					"because memory is short. The sweep will be "
					"slower.\n",
					size);
			}
			return true;
		}
	}
	return false;
}

/**
 * Frees the resources held by the thread state. Country names that were
 * moved into the merged table are owned by that table and are skipped.
 * @param state to free, which may be partially created.
 * @param mergedNames country names now owned by the merged table, or
 * null when nothing was merged.
 */
static void threadStateFree(
	ThreadState* state,
	char* const mergedNames[COUNTRY_SLOTS]) {
	if (state->results != NULL) {
		ResultsIpiFree(state->results);
		state->results = NULL;
	}
	if (state->cache != NULL) {
		Free(state->cache);
		state->cache = NULL;
	}
	sharedBlocksFree(&state->sharedBlocks);
	for (int c = 0; c < COUNTRY_SLOTS; c++) {
		if (state->countryNames[c] != NULL &&
			(mergedNames == NULL ||
			state->countryNames[c] != mergedNames[c])) {
			Free(state->countryNames[c]);
		}
		state->countryNames[c] = NULL;
	}
}

/**
 * Frees every thread state and the thread arrays after a failure before
 * any results were merged. Either array may be null.
 */
static void threadStatesFree(
	ThreadState* states,
	FIFTYONE_DEGREES_THREAD* threads,
	int threadCount) {
	if (states != NULL) {
		for (int t = 0; t < threadCount; t++) {
			threadStateFree(&states[t], NULL);
		}
		Free(states);
	}
	if (threads != NULL) {
		Free(threads);
	}
}

/**
 * Finds the component id that the property belongs to so the correct
 * graphs can be evaluated for each address. Distinct component ids are
 * accumulated in the shared state.
 */
static bool addComponentForProperty(
	SharedState* shared,
	const char* propertyName) {
	DataSetIpi* dataSet = shared->dataSet;
	EXCEPTION_CREATE;
	Item item;
	DataReset(&item.data);
	const int required = PropertiesGetRequiredPropertyIndexFromName(
		dataSet->b.b.available,
		propertyName);
	if (required < 0) {
		return false;
	}
	const uint32_t propertyIndex =
		dataSet->b.b.available->items[required].propertyIndex;
	const Property* property = PropertyGet(
		dataSet->properties,
		propertyIndex,
		&item,
		exception);
	if (property == NULL) {
		return false;
	}
	// The item holds the property whenever it is returned, even when the
	// exception is also set, so it is released before checking for the
	// failure to avoid leaking it.
	const bool failed = EXCEPTION_FAILED;
	byte componentId = 0;
	if (failed == false) {
		const Component* component = (const Component*)dataSet
			->componentsList.items[property->componentIndex].data.ptr;
		componentId = component->componentId;
	}
	COLLECTION_RELEASE(dataSet->properties, &item);
	if (failed == true) {
		return false;
	}
	for (int c = 0; c < shared->componentCount; c++) {
		if (shared->componentIds[c] == componentId) {
			return true;
		}
	}
	if (shared->componentCount >= MAX_COMPONENTS) {
		return false;
	}
	shared->componentIds[shared->componentCount++] = componentId;
	return true;
}

/**
 * Returns the required property index for the name, reporting a message
 * when the property is not available in the data file.
 */
static int getRequiredIndex(DataSetIpi* dataSet, const char* name) {
	const int index = PropertiesGetRequiredPropertyIndexFromName(
		dataSet->b.b.available,
		name);
	if (index < 0) {
		printf(
			"The data file does not include the required property "
			"'%s'. An enterprise IP Intelligence data file that "
			"includes the weighted country code properties is "
			"required. To obtain one see "
			"https://51degrees.com/pricing?utm_source=code&utm_medium=example&utm_campaign=ip-intelligence-cxx&utm_content=examples-c-ipintelligence-countryoverlap.c&utm_term=missing-property\n",
			name);
	}
	return index;
}

/**
 * Writes the CSV pairing each primary country with the secondary
 * countries its addresses can also resolve to, for each combination of
 * location confidence and connection type. The row where the secondary
 * country equals the primary country counts the addresses that resolve
 * entirely to the primary country. The proportion, where 1 is 100%, is
 * of all the addresses whose primary country is the row's primary
 * country across every location confidence and connection type.
 */
static void writeCombinedCsv(
	FILE* file,
	Matrix cells,
	SharedBlocks* sharedBlocks,
	char* countryNames[COUNTRY_SLOTS],
	ValueTable* confidence,
	ValueTable* connection) {
	char buffer[8];
	char secondaryBuffer[8];

	// Total addresses for each primary country in any variant of
	// location confidence and connection type. Used as the denominator
	// for every percentage.
	uint64_t primaryTotals[COUNTRY_SLOTS];
	memset(primaryTotals, 0, sizeof(primaryTotals));
	for (int c = 0; c < COUNTRY_SLOTS; c++) {
		for (int f = 0; f < confidence->count; f++) {
			for (int n = 0; n < connection->count; n++) {
				primaryTotals[c] += cells[c][f][n].addresses;
			}
		}
	}

	fprintf(
		file,
		"PrimaryCountryCode,PrimaryCountry,SecondaryCountryCode,"
		"SecondaryCountry,LocationConfidence,ConnectionType,"
		"IpAddressCount,ProportionOfPrimaryCountryAddresses\n");
	for (int c = 0; c < COUNTRY_SLOTS; c++) {
		const double total = (double)primaryTotals[c];
		for (int f = 0; f < confidence->count; f++) {
			for (int n = 0; n < connection->count; n++) {
				const Cell* cell = &cells[c][f][n];
				if (cell->addresses == 0) {
					continue;
				}
				// The row where the primary and secondary country are
				// the same counts the addresses that resolve entirely
				// to the primary country. The ISO code stands in for
				// a name the sweep never captured, which happens when
				// the country only ever appears as a secondary.
				char nameBuffer[8];
				const char* primaryName = countryNames[c] != NULL ?
					countryNames[c] : countryCode(c, nameBuffer);
				fprintf(
					file,
					"%s,\"%s\",%s,\"%s\",%s,%s,%llu,%.9g\n",
					countryCode(c, buffer),
					primaryName,
					countryCode(c, secondaryBuffer),
					primaryName,
					confidence->names[f],
					connection->names[n],
					(unsigned long long)cell->single,
					total > 0 ?
					(double)cell->single / total : 0);
				// One row per secondary country in descending order of
				// the addresses that can resolve to the combination.
				const uint64_t* block = sharedBlocks->blocks[f][n];
				if (block == NULL) {
					continue;
				}
				const uint64_t* row =
					block + (size_t)c * COUNTRY_SLOTS;
				bool used[COUNTRY_SLOTS];
				memset(used, 0, sizeof(used));
				for (;;) {
					int best = -1;
					uint64_t bestCount = 0;
					for (int s = 0; s < COUNTRY_SLOTS; s++) {
						if (used[s] == false && row[s] > bestCount) {
							bestCount = row[s];
							best = s;
						}
					}
					if (best < 0) {
						break;
					}
					used[best] = true;
					char secondaryNameBuffer[8];
					fprintf(
						file,
						"%s,\"%s\",%s,\"%s\",%s,%s,%llu,%.9g\n",
						countryCode(c, buffer),
						primaryName,
						countryCode(best, secondaryBuffer),
						countryNames[best] != NULL ?
						countryNames[best] :
						countryCode(best, secondaryNameBuffer),
						confidence->names[f],
						connection->names[n],
						(unsigned long long)bestCount,
						total > 0 ?
						(double)bestCount / total : 0);
				}
			}
		}
	}
}

/**
 * Prints the countries with the highest overlap share, optionally
 * restricted to a single connection type index, or -1 for all. When the
 * shared counts are provided the top shared countries are named for each
 * country in the table.
 */
static void printTop(
	Matrix cells,
	SharedBlocks* sharedBlocks,
	char* countryNames[COUNTRY_SLOTS],
	ValueTable* confidence,
	ValueTable* connection,
	int connectionFilter,
	int count) {
	char buffer[8];
	char sharedBuffer[8];
	printf(
		"%-5s %-30s %15s %15s %7s  %s\n",
		"Code",
		"Country",
		"Addresses",
		"Overlapping",
		"Share",
		sharedBlocks != NULL ? "Top shared countries" : "");
	bool printed[COUNTRY_UNKNOWN];
	memset(printed, 0, sizeof(printed));
	for (int rank = 0; rank < count; rank++) {
		int bestCountry = -1;
		double bestShare = -1;
		uint64_t bestAddresses = 0;
		uint64_t bestOverlapping = 0;
		for (int c = 0; c < COUNTRY_UNKNOWN; c++) {
			if (printed[c] == true) {
				continue;
			}
			uint64_t addresses = 0;
			uint64_t overlapping = 0;
			for (int f = 0; f < confidence->count; f++) {
				for (int n = 0; n < connection->count; n++) {
					if (connectionFilter >= 0 &&
						n != connectionFilter) {
						continue;
					}
					addresses += cells[c][f][n].addresses;
					overlapping += cells[c][f][n].multi;
				}
			}
			if (addresses < 1000000) {
				continue;
			}
			const double share = (double)overlapping / (double)addresses;
			if (share > bestShare) {
				bestShare = share;
				bestCountry = c;
				bestAddresses = addresses;
				bestOverlapping = overlapping;
			}
		}
		if (bestCountry < 0) {
			break;
		}
		printed[bestCountry] = true;
		printf(
			"%-5s %-30s %15llu %15llu %6.2f%%  ",
			countryCode(bestCountry, buffer),
			countryNames[bestCountry] != NULL ?
			countryNames[bestCountry] : "",
			(unsigned long long)bestAddresses,
			(unsigned long long)bestOverlapping,
			100.0 * bestShare);
		if (sharedBlocks != NULL) {
			// Name every shared country for the country across all
			// the confidence values and connection types, highest
			// shared address counts first.
			bool used[COUNTRY_SLOTS];
			memset(used, 0, sizeof(used));
			for (int top = 0; ; top++) {
				int best = -1;
				uint64_t bestCount = 0;
				for (int s = 0; s < COUNTRY_SLOTS; s++) {
					if (used[s] == true) {
						continue;
					}
					uint64_t value = 0;
					for (int f = 0; f < confidence->count; f++) {
						for (int n = 0; n < connection->count; n++) {
							const uint64_t* block =
								sharedBlocks->blocks[f][n];
							if (block != NULL) {
								value += block[
									(size_t)bestCountry *
									COUNTRY_SLOTS + s];
							}
						}
					}
					if (value > bestCount) {
						bestCount = value;
						best = s;
					}
				}
				if (best < 0) {
					break;
				}
				used[best] = true;
				printf(
					"%s%s %.1f%%",
					top > 0 ? ", " : "",
					countryCode(best, sharedBuffer),
					100.0 * (double)bestCount /
					(double)bestAddresses);
			}
		}
		printf("\n");
	}
}

/** Prints overall statistics and top countries per connection type. */
static void printSummary(
	Matrix cells,
	SharedBlocks* sharedBlocks,
	char* countryNames[COUNTRY_SLOTS],
	ValueTable* confidence,
	ValueTable* connection) {
	uint64_t total = 0;
	uint64_t multi = 0;
	uint64_t multiArea = 0;
	for (int c = 0; c < COUNTRY_UNKNOWN; c++) {
		for (int f = 0; f < confidence->count; f++) {
			for (int n = 0; n < connection->count; n++) {
				total += cells[c][f][n].addresses;
				multi += cells[c][f][n].multi;
				multiArea += cells[c][f][n].multiArea;
			}
		}
	}
	printf(
		"\nIPv4 addresses that resolved to a country: %llu\n",
		(unsigned long long)total);
	printf(
		"Of which the people within the area also include one or more "
		"additional countries: %llu (%.2f%%)\n",
		(unsigned long long)multi,
		total > 0 ? 100.0 * (double)multi / (double)total : 0);
	// The area weighted figure is shown for comparison only. The two
	// differ most where an area covers sparsely populated terrain
	// across a border.
	printf(
		"For comparison, by share of the area rather than of the "
		"people: %llu (%.2f%%)\n",
		(unsigned long long)multiArea,
		total > 0 ? 100.0 * (double)multiArea / (double)total : 0);

	// Overall share for each connection type.
	printf("\nBy connection type:\n");
	for (int n = 0; n < connection->count; n++) {
		uint64_t typeTotal = 0;
		uint64_t typeMulti = 0;
		for (int c = 0; c < COUNTRY_UNKNOWN; c++) {
			for (int f = 0; f < confidence->count; f++) {
				typeTotal += cells[c][f][n].addresses;
				typeMulti += cells[c][f][n].multi;
			}
		}
		if (typeTotal == 0) {
			continue;
		}
		printf(
			"  %-22s %15llu addresses, %6.2f%% overlap additional "
			"countries\n",
			connection->names[n],
			(unsigned long long)typeTotal,
			100.0 * (double)typeMulti / (double)typeTotal);
	}

	printf(
		"\nCountries with at least 1 million addresses ordered by share "
		"of addresses that overlap additional countries (top 20, all "
		"connection types):\n");
	printTop(
		cells,
		sharedBlocks,
		countryNames,
		confidence,
		connection,
		-1,
		20);
	for (int n = 0; n < connection->count; n++) {
		if (strcmp(connection->names[n], "Cellular") == 0 ||
			strcmp(connection->names[n], "Broadband") == 0) {
			printf(
				"\nTop 10 for connection type '%s':\n",
				connection->names[n]);
			printTop(
				cells,
				NULL,
				countryNames,
				confidence,
				connection,
				n,
				10);
		}
	}
}

/**
 * Runs the country overlap analysis.
 * @param dataFilePath path to the enterprise data file.
 * @param configProvided configuration to use, or null to use the
 * Balanced configuration falling back to LowMemory when memory is not
 * available.
 * @param outputPath path for the output CSV.
 * @param threadCount number of worker threads.
 * @param firstChunk first chunk of the IPv4 address space to process,
 * where chunk 0 starts at 0.0.0.0 and each chunk is a /8 by default.
 * @param totalChunks number of chunks to process starting at
 * firstChunk, 256 with a firstChunk of 0 for the whole space.
 * @param minSecondaryPercent secondary countries whose share of the
 * population weighting is this percentage or lower are ignored. Zero
 * includes every listed country.
 * @return COUNTRY_OVERLAP_OK on success,
 * COUNTRY_OVERLAP_PROPERTIES_MISSING when the data file does not include
 * the required properties, or COUNTRY_OVERLAP_FAILED on any other
 * failure.
 */
int fiftyoneDegreesIpiCountryOverlap(
	const char* dataFilePath,
	fiftyoneDegreesConfigIpi* configProvided,
	const char* outputPath,
	int threadCount,
	int firstChunk,
	int totalChunks,
	double minSecondaryPercent) {
	if (threadCount < 1) {
		threadCount = 1;
	}
	if (firstChunk < 0 || firstChunk >= MAX_CHUNKS) {
		firstChunk = 0;
	}
	if (totalChunks < 1 || totalChunks > MAX_CHUNKS - firstChunk) {
		totalChunks = MAX_CHUNKS - firstChunk;
	}
	if (minSecondaryPercent < 0) {
		minSecondaryPercent = 0;
	}
	if (minSecondaryPercent > 100) {
		minSecondaryPercent = 100;
	}

	ResourceManager manager;
	EXCEPTION_CREATE;
	const uint16_t concurrency = (uint16_t)(threadCount + 2);
	PropertiesRequired properties = PropertiesDefault;
	properties.string = countryOverlapProperties;

	// The Balanced configuration loads the graph structures into memory
	// which gives the best sweep throughput but requires several GB of
	// committed memory for a large data file. When no configuration is
	// provided and that allocation fails, the example falls back to the
	// LowMemory configuration with enlarged caches. The operating system
	// file cache keeps the hot parts of the data file in physical memory
	// so the fallback remains fast for the sequential access pattern of
	// the sweep.
	StatusCode status = NOT_SET;
	const int attempts = configProvided != NULL ? 1 : 2;
	for (int attempt = 0; attempt < attempts; attempt++) {
		ConfigIpi config;
		if (configProvided != NULL) {
			config = *configProvided;
			// The sweep evaluates the graph for billions of addresses
			// and resolves hundreds of thousands of distinct locations,
			// so a configuration that neither loads nor caches the
			// collections involved would spend hours re-reading the
			// same records from disk. Enforce minimum caches on any
			// collection that has neither, so every standard
			// configuration remains practical for the sweep.
			bool adjusted = false;
#define COUNTRY_OVERLAP_MIN_CACHE(collection, size) \
			if (config.collection.loaded == false && \
				config.collection.capacity == 0) { \
				config.collection.capacity = size; \
				adjusted = true; \
			}
			COUNTRY_OVERLAP_MIN_CACHE(graph, 200000)
			COUNTRY_OVERLAP_MIN_CACHE(graphs, 1000)
			COUNTRY_OVERLAP_MIN_CACHE(strings, 50000)
			COUNTRY_OVERLAP_MIN_CACHE(values, 10000)
			COUNTRY_OVERLAP_MIN_CACHE(profiles, 10000)
			COUNTRY_OVERLAP_MIN_CACHE(profileGroups, 50000)
			COUNTRY_OVERLAP_MIN_CACHE(profileOffsets, 10000)
			COUNTRY_OVERLAP_MIN_CACHE(propertyTypes, 1000)
			COUNTRY_OVERLAP_MIN_CACHE(properties, 1000)
			COUNTRY_OVERLAP_MIN_CACHE(components, 100)
			COUNTRY_OVERLAP_MIN_CACHE(maps, 100)
#undef COUNTRY_OVERLAP_MIN_CACHE
			if (adjusted == true) {
				printf(
					"The provided configuration leaves collections "
					"without caches, using minimum caches for the "
					"sweep.\n");
			}
		}
		else if (attempt == 0) {
			config = IpiBalancedConfig;
		}
		else {
			printf(
				"Balanced configuration failed, falling back to the "
				"LowMemory configuration with enlarged caches.\n");
			config = IpiLowMemoryConfig;
			config.graph.capacity = 200000;
			config.graphs.capacity = 1000;
			config.profileGroups.capacity = 50000;
			config.profileOffsets.capacity = 10000;
			config.profiles.capacity = 10000;
			config.values.capacity = 10000;
			config.strings.capacity = 50000;
			config.properties.capacity = 1000;
			config.components.capacity = 100;
			config.maps.capacity = 100;
			config.propertyTypes.capacity = 1000;
		}
		// Size the caches for the expected concurrency.
		config.strings.concurrency = concurrency;
		config.components.concurrency = concurrency;
		config.maps.concurrency = concurrency;
		config.properties.concurrency = concurrency;
		config.values.concurrency = concurrency;
		config.profiles.concurrency = concurrency;
		config.graphs.concurrency = concurrency;
		config.profileGroups.concurrency = concurrency;
		config.profileOffsets.concurrency = concurrency;
		config.propertyTypes.concurrency = concurrency;
		config.graph.concurrency = concurrency;

		EXCEPTION_CLEAR;
		status = IpiInitManagerFromFile(
			&manager,
			&config,
			&properties,
			dataFilePath,
			exception);
		// Both the status and the exception must be clean. The status
		// can report success while the exception carries a failure from
		// the graph structures, in which case the manager must not be
		// used.
		if (status == SUCCESS && EXCEPTION_OKAY) {
			break;
		}
		if (status == SUCCESS) {
			status = countryOverlapExceptionStatus(exception);
		}
		// When none of the required properties exist in the data file
		// there is no point retrying with another configuration. This
		// is the expected outcome for the Lite data file.
		if (status == REQ_PROP_NOT_PRESENT) {
			printf(
				"The data file does not include the properties required "
				"by this example (%s). An enterprise IP Intelligence "
				"data file is required. To obtain one see "
				"https://51degrees.com/pricing?utm_source=code&utm_medium=example&utm_campaign=ip-intelligence-cxx&utm_content=examples-c-ipintelligence-countryoverlap.c&utm_term=properties-missing\n",
				countryOverlapProperties);
			return COUNTRY_OVERLAP_PROPERTIES_MISSING;
		}
	}
	if (status != SUCCESS || EXCEPTION_FAILED) {
		countryOverlapReportStatus(status, dataFilePath);
		printf(
			"The data file could not be initialised. This can happen "
			"when the machine does not have enough free committed "
			"memory for the selected configuration.\n");
		return COUNTRY_OVERLAP_FAILED;
	}

	DataSetIpi* dataSet = DataSetIpiGet(&manager);

	// Stop when any required property is missing from the data file. The
	// analysis is meaningless without the weighted country code lists,
	// which the Lite data file does not include.
	SharedState shared;
	memset(&shared, 0, sizeof(shared));
	shared.manager = &manager;
	shared.dataSet = dataSet;
	shared.nextChunk = firstChunk;
	shared.endChunk = firstChunk + totalChunks;
	shared.minShare = minSecondaryPercent / 100.0;
	shared.reqIndexCountryCode = getRequiredIndex(
		dataSet, PROPERTY_COUNTRY_CODE);
	shared.reqIndexCountry = getRequiredIndex(dataSet, PROPERTY_COUNTRY);
	shared.reqIndexConfidence = getRequiredIndex(
		dataSet, PROPERTY_CONFIDENCE);
	shared.reqIndexConnection = getRequiredIndex(
		dataSet, PROPERTY_CONNECTION);
	shared.reqIndexGeo = getRequiredIndex(dataSet, PROPERTY_GEO);
	shared.reqIndexPop = getRequiredIndex(dataSet, PROPERTY_POP);
	if (shared.reqIndexCountryCode < 0 ||
		shared.reqIndexCountry < 0 ||
		shared.reqIndexConfidence < 0 ||
		shared.reqIndexConnection < 0 ||
		shared.reqIndexGeo < 0 ||
		shared.reqIndexPop < 0) {
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_PROPERTIES_MISSING;
	}

	// Determine the distinct component graphs that the properties read
	// by the analysis depend on. Values are reused for every address in
	// a segment with the same graph offsets, so a component left out of
	// the key would let a cached result stand in for an address whose
	// values from that component differ.
	const char* keyProperties[] = {
		PROPERTY_COUNTRY_CODE,
		PROPERTY_COUNTRY,
		PROPERTY_CONFIDENCE,
		PROPERTY_CONNECTION,
		PROPERTY_GEO,
		PROPERTY_POP
	};
	for (size_t i = 0;
		i < sizeof(keyProperties) / sizeof(keyProperties[0]);
		i++) {
		if (addComponentForProperty(&shared, keyProperties[i]) == false) {
			printf(
				"Could not determine the component for property '%s', "
				"or the required properties span more than the %d "
				"component graphs the segment key supports.\n",
				keyProperties[i],
				MAX_COMPONENTS);
			DataSetIpiRelease(dataSet);
			ResourceManagerFree(&manager);
			return COUNTRY_OVERLAP_FAILED;
		}
	}

	// Open the output before the sweep so that a path that cannot be
	// written fails at once rather than after an hour of work.
	FILE* file = fopen(outputPath, "w");
	if (file == NULL) {
		printf("Could not open '%s' for writing.\n", outputPath);
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_FAILED;
	}

	printf(
		"Sweeping %d /%d chunks of the IPv4 address space with %d "
		"threads evaluating %d component graphs per address. Secondary "
		"countries with a weighting share of %g%% or lower are "
		"ignored. Each thread uses a cache of up to %.0f MB, plus %.1f MB "
		"for every combination of location confidence and connection type "
		"it meets.\n",
		totalChunks,
		32 - COUNTRY_OVERLAP_CHUNK_BITS,
		threadCount,
		shared.componentCount,
		minSecondaryPercent,
		(double)sizeof(Resolved) * CACHE_SIZE / (1024.0 * 1024.0),
		(double)sizeof(uint64_t) * COUNTRY_SLOTS * COUNTRY_SLOTS /
		(1024.0 * 1024.0));

	// Create the thread states and start the workers. Each allocation is
	// checked because the caches alone need several GB at the default
	// thread count, and a machine short of memory must get a clear
	// message rather than a crash.
	ThreadState* states = (ThreadState*)Malloc(
		sizeof(ThreadState) * threadCount);
	FIFTYONE_DEGREES_THREAD* threads = (FIFTYONE_DEGREES_THREAD*)Malloc(
		sizeof(FIFTYONE_DEGREES_THREAD) * threadCount);
	bool created = states != NULL && threads != NULL;
	if (states != NULL) {
		// Zeroed first so the free below is safe for states that were
		// never fully created.
		memset(states, 0, sizeof(ThreadState) * threadCount);
	}
	for (int t = 0; t < threadCount && created == true; t++) {
		ThreadState* state = &states[t];
		state->shared = &shared;
		state->results = ResultsIpiCreate(&manager);
		created = state->results != NULL && threadCacheCreate(state);
	}
	if (created == false) {
		printf(
			"Not enough memory for %d threads. Try fewer threads.\n",
			threadCount);
		threadStatesFree(states, threads, threadCount);
		fclose(file);
		remove(outputPath);
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_FAILED;
	}
	time_t started = time(NULL);
	int startedThreads = 0;
	while (startedThreads < threadCount &&
		threadStart(&threads[startedThreads], &states[startedThreads]) ==
		true) {
		startedThreads++;
	}
	if (startedThreads < threadCount) {
		// The progress loop waits for every worker to finish, so without
		// this it would wait forever for a thread that never ran. The
		// workers that did start are told to stop and are joined before
		// everything is freed.
		printf(
			"Could not start thread %d of %d. Try fewer threads.\n",
			startedThreads + 1,
			threadCount);
		FIFTYONE_DEGREES_INTERLOCK_INC(&shared.stopRequested);
		for (int t = 0; t < startedThreads; t++) {
			FIFTYONE_DEGREES_THREAD_JOIN(threads[t]);
			FIFTYONE_DEGREES_THREAD_CLOSE(threads[t]);
		}
		threadStatesFree(states, threads, threadCount);
		fclose(file);
		remove(outputPath);
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_FAILED;
	}

	// Report progress every 30 seconds until all the threads have
	// finished. The wait is taken one second at a time so that a short
	// sweep, such as the one the tests run, is not held up for the full
	// reporting interval.
	int secondsSinceReport = 0;
	while (shared.threadsDone < threadCount) {
		sleepOneSecond();
		secondsSinceReport++;
		if (shared.threadsDone >= threadCount || secondsSinceReport < 30) {
			continue;
		}
		secondsSinceReport = 0;
		uint64_t done = 0;
		for (int t = 0; t < threadCount; t++) {
			done += states[t].addressesDone;
		}
		double progress = (double)done /
			((double)CHUNK_SIZE * totalChunks);
		double elapsed = difftime(time(NULL), started);
		double remaining = progress > 0 ?
			elapsed * (1 - progress) / progress : 0;
		printf(
			"Covered %.1f%% of the target address space in %.0fs, "
			"estimated %.0fs remaining.\n",
			progress * 100,
			elapsed,
			remaining);
		fflush(stdout);
	}
	for (int t = 0; t < threadCount; t++) {
		FIFTYONE_DEGREES_THREAD_JOIN(threads[t]);
		FIFTYONE_DEGREES_THREAD_CLOSE(threads[t]);
	}
	double elapsed = difftime(time(NULL), started);

	// The caches are not read again. The merge, the CSV, the spot checks
	// and the summary all work from the matrices and the normal lookup
	// process, so freeing the caches here keeps the peak below the sum of
	// the caches and the pair count blocks the merge is about to allocate.
	for (int t = 0; t < threadCount; t++) {
		if (states[t].cache != NULL) {
			Free(states[t].cache);
			states[t].cache = NULL;
		}
	}

	// Merge the thread results using global dimension tables so the
	// indexes align across threads. The merge tables are static because
	// they are too large for the stack.
	static Matrix totals;
	static char* names[COUNTRY_SLOTS];
	SharedBlocks totalShared;
	ValueTable confidence;
	ValueTable connection;
	memset(&confidence, 0, sizeof(confidence));
	memset(&connection, 0, sizeof(connection));
	memset(totals, 0, sizeof(totals));
	memset(&totalShared, 0, sizeof(totalShared));
	memset(names, 0, sizeof(names));
	uint64_t evaluations = 0;
	uint64_t resolutions = 0;
	uint64_t failures = 0;
	uint64_t truncations = 0;
	uint64_t pairBlockFailures = 0;
	for (int t = 0; t < threadCount; t++) {
		ThreadState* state = &states[t];
		evaluations += state->evaluations;
		resolutions += state->resolutions;
		failures += state->failures;
		truncations += state->truncations;
		pairBlockFailures += state->pairBlockFailures;
		for (int c = 0; c < COUNTRY_SLOTS; c++) {
			if (names[c] == NULL && state->countryNames[c] != NULL) {
				names[c] = state->countryNames[c];
			}
			for (int f = 0; f < state->confidence.count; f++) {
				const int globalF = valueIndex(
					&confidence,
					state->confidence.names[f]);
				for (int n = 0; n < state->connection.count; n++) {
					const Cell* cell = &state->cells[c][f][n];
					if (cell->addresses == 0 && cell->segments == 0) {
						continue;
					}
					Cell* target = &totals
						[c]
						[globalF]
						[valueIndex(
							&connection,
							state->connection.names[n])];
					target->segments += cell->segments;
					target->addresses += cell->addresses;
					target->single += cell->single;
					target->multi += cell->multi;
					target->multiArea += cell->multiArea;
				}
			}
		}
		// Merge the pair count blocks aligning the dimension indexes
		// with the global tables.
		for (int f = 0; f < state->confidence.count; f++) {
			const int globalF = valueIndex(
				&confidence,
				state->confidence.names[f]);
			for (int n = 0; n < state->connection.count; n++) {
				const uint64_t* source = state->sharedBlocks.blocks[f][n];
				if (source == NULL) {
					continue;
				}
				uint64_t* target = sharedBlockGet(
					&totalShared,
					globalF,
					valueIndex(&connection, state->connection.names[n]));
				if (target != NULL) {
					const size_t entries =
						(size_t)COUNTRY_SLOTS * COUNTRY_SLOTS;
					for (size_t i = 0; i < entries; i++) {
						target[i] += source[i];
					}
				}
				else {
					// As in addSegment: without the block the merged CSV
					// silently loses the secondary country rows for this
					// combination.
					pairBlockFailures++;
				}
			}
		}
	}

	printf(
		"\nCompleted %llu graph evaluations with %llu distinct "
		"combinations resolved and %llu failures in %.0f seconds.\n",
		(unsigned long long)evaluations,
		(unsigned long long)resolutions,
		(unsigned long long)failures,
		elapsed);
	if (truncations > 0) {
		printf(
			"%llu distinct locations overlapped more than %d other "
			"countries, so the pair counts for those locations exclude "
			"the lowest weighted countries.\n",
			(unsigned long long)truncations,
			SHARED_STORE);
	}
	if (pairBlockFailures > 0) {
		printf(
			"%llu pair count updates were lost because a country pair "
			"block could not be allocated, so the CSV is missing secondary "
			"country rows. Try fewer threads.\n",
			(unsigned long long)pairBlockFailures);
	}

	writeCombinedCsv(
		file,
		totals,
		&totalShared,
		names,
		&confidence,
		&connection);
	// A write error, such as a full disk, is only visible through the
	// stream's error flag and the result of closing it.
	bool csvWritten = ferror(file) == 0;
	if (fclose(file) != 0) {
		csvWritten = false;
	}
	if (csvWritten == true) {
		printf("Results written to '%s'.\n", outputPath);
	}
	else {
		printf("Could not write the results to '%s'.\n", outputPath);
	}
	if (failures > 0) {
		printf(
			"%llu graph evaluations or resolutions failed, so those "
			"addresses are counted in the Failed rows and the results "
			"are incomplete.\n",
			(unsigned long long)failures);
	}

	// Spot check randomly selected addresses against the normal lookup
	// process so the method can be seen to produce the same values as a
	// single lookup would. The seed is fixed so runs are reproducible.
	printf(
		"\nSpot checking %d random addresses against the normal lookup "
		"process:\n",
		SPOT_CHECKS);
	srand(42);
	int matches = 0;
	for (int i = 0; i < SPOT_CHECKS; i++) {
		const uint32_t chunk =
			(uint32_t)(firstChunk + rand() % totalChunks);
		const uint32_t withinChunk =
			(((uint32_t)rand() << 9) | ((uint32_t)rand() & 0x1FF)) &
			(uint32_t)(CHUNK_SIZE - 1);
		if (spotCheck(
			&states[0],
			(chunk << COUNTRY_OVERLAP_CHUNK_BITS) | withinChunk) == true) {
			matches++;
		}
	}
	printf(
		"%d of %d spot checks match the normal lookup process.\n",
		matches,
		SPOT_CHECKS);

	printSummary(totals, &totalShared, names, &confidence, &connection);

	// Free all the resources.
	for (int t = 0; t < threadCount; t++) {
		threadStateFree(&states[t], names);
	}
	for (int c = 0; c < COUNTRY_SLOTS; c++) {
		if (names[c] != NULL) {
			Free(names[c]);
			names[c] = NULL;
		}
	}
	sharedBlocksFree(&totalShared);
	Free(states);
	Free(threads);
	DataSetIpiRelease(dataSet);
	ResourceManagerFree(&manager);
	// The run only succeeds when the complete results were written and
	// the spot checks confirm them. A lost pair count block leaves the CSV
	// without the secondary country rows it exists to report, so it fails
	// the run in the same way as a write error.
	return csvWritten == true &&
		failures == 0 &&
		pairBlockFailures == 0 &&
		matches == SPOT_CHECKS ?
		COUNTRY_OVERLAP_OK : COUNTRY_OVERLAP_FAILED;
}

#ifndef TEST

static const char* dataDir = "ip-intelligence-data";

/** The enterprise data file is searched for first because the analysis
requires properties the Lite file does not include. The Lite name is
still searched so a clear message about the missing properties can be
given rather than a file not found error. */
static const char* dataFileNames[] = {
	"51Degrees-EnterpriseIpiV41.ipi",
	"51Degrees-LiteV41.ipi",
};

/**
 * Counts the distinct countries for the required property index whose
 * summed share of the list's total weighting exceeds minShare. Used by
 * probe mode to show how many countries each list contains.
 */
static int countDistinctAbove(
	const WeightedValuesCollection* collection,
	int requiredPropertyIndex,
	double minShare) {
	CountryWeights weights;
	sumCountryWeights(&weights, collection, requiredPropertyIndex);
	int above = 0;
	for (int s = 0; s < weights.count; s++) {
		if (weights.weights[s] > weights.total * minShare) {
			above++;
		}
	}
	return above;
}

/**
 * Returns false when the text is written as an IPv4 address but is not
 * four decimal octets of at most 255 each. Text containing a colon is
 * IPv6 and is left for the library parser to validate.
 */
static bool isValidIpv4OrOther(const char* ip) {
	if (strchr(ip, ':') != NULL) {
		return true;
	}
	unsigned int octets[4];
	char trailing;
	if (sscanf(
		ip,
		"%u.%u.%u.%u%c",
		&octets[0],
		&octets[1],
		&octets[2],
		&octets[3],
		&trailing) != 4) {
		return false;
	}
	for (int i = 0; i < 4; i++) {
		if (octets[i] > 255) {
			return false;
		}
	}
	return true;
}

/**
 * Prints the resolved values for a single IPv4 or IPv6 address so the
 * results can be compared against other 51Degrees APIs for verification.
 */
static void probeAddress(
	SharedState* shared,
	ResultsIpi* results,
	const char* ip) {
	// The library parser is used because it handles IPv6 as well as
	// IPv4. It clamps an IPv4 octet above 255 to 255 rather than
	// rejecting it, which would probe a different address from the one
	// requested, so IPv4 octets are checked first.
	fiftyoneDegreesIpAddress address;
	memset(&address, 0, sizeof(address));
	if (isValidIpv4OrOther(ip) == false ||
		fiftyoneDegreesIpAddressParse(
			ip,
			ip + strlen(ip),
			&address) == false) {
		printf("'%s' is not an IPv4 or IPv6 address.\n", ip);
		return;
	}
	EXCEPTION_CREATE;
	ResultsIpiFromIpAddress(
		results,
		address.value,
		address.type == FIFTYONE_DEGREES_IP_TYPE_IPV4 ?
		FIFTYONE_DEGREES_IPV4_LENGTH : FIFTYONE_DEGREES_IPV6_LENGTH,
		(fiftyoneDegreesIpType)address.type,
		exception);
	EXCEPTION_THROW;
	WeightedValuesCollection collection = ResultsIpiGetValuesCollection(
		results,
		NULL,
		0,
		NULL,
		exception);
	EXCEPTION_THROW;
	const char* code = highestWeighted(
		&collection, shared->reqIndexCountryCode);
	const char* conf = highestWeighted(
		&collection, shared->reqIndexConfidence);
	const char* conn = highestWeighted(
		&collection, shared->reqIndexConnection);
	printf(
		"%s cc=%s lc=%s ct=%s",
		ip,
		code != NULL ? code : "Unknown",
		conf != NULL ? conf : "Unknown",
		conn != NULL ? conn : "Unknown");
	// Both weighted country lists are printed so the area weighted and
	// population weighted views can be compared for the same address.
	// Weights are displayed as each value's share of the total weight
	// for the property, which is stable across the raw weighting scales
	// used by different versions of the API.
	const int lists[2] = { shared->reqIndexGeo, shared->reqIndexPop };
	const char* labels[2] = { " geo=[", " pop=[" };
	for (int list = 0; list < 2; list++) {
		printf("%s", labels[list]);
		double totalWeight = 0;
		for (uint32_t i = 0; i < collection.itemsCount; i++) {
			const fiftyoneDegreesWeightedValueHeader* header =
				collection.items[i];
			if (header->requiredPropertyIndex == lists[list]) {
				totalWeight += (double)header->rawWeighting;
			}
		}
		for (uint32_t i = 0; i < collection.itemsCount; i++) {
			const fiftyoneDegreesWeightedValueHeader* header =
				collection.items[i];
			if (header->requiredPropertyIndex == lists[list] &&
				header->valueType ==
				FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING &&
				totalWeight > 0) {
				printf(
					"%s:%.3f|",
					((const fiftyoneDegreesWeightedString*)header)->value,
					(double)header->rawWeighting / totalWeight);
			}
		}
		printf("]");
	}
	printf(" geoDistinct=%d popDistinct=%d\n",
		countDistinctAbove(&collection, shared->reqIndexGeo, 0),
		countDistinctAbove(&collection, shared->reqIndexPop, 0));
	WeightedValuesCollectionRelease(&collection);
}

/**
 * Initialises the manager and prints the values for each of the
 * addresses provided on the command line.
 */
static int runProbe(const char* dataFilePath, int argc, char* argv[]) {
	ResourceManager manager;
	EXCEPTION_CREATE;
	ConfigIpi config = IpiLowMemoryConfig;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = countryOverlapProperties;
	StatusCode status = IpiInitManagerFromFile(
		&manager,
		&config,
		&properties,
		dataFilePath,
		exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		countryOverlapReportStatus(status, dataFilePath);
		return COUNTRY_OVERLAP_FAILED;
	}
	DataSetIpi* dataSet = DataSetIpiGet(&manager);
	SharedState shared;
	memset(&shared, 0, sizeof(shared));
	shared.manager = &manager;
	shared.dataSet = dataSet;
	shared.reqIndexCountryCode = getRequiredIndex(
		dataSet, PROPERTY_COUNTRY_CODE);
	shared.reqIndexCountry = getRequiredIndex(dataSet, PROPERTY_COUNTRY);
	shared.reqIndexConfidence = getRequiredIndex(
		dataSet, PROPERTY_CONFIDENCE);
	shared.reqIndexConnection = getRequiredIndex(
		dataSet, PROPERTY_CONNECTION);
	shared.reqIndexGeo = getRequiredIndex(dataSet, PROPERTY_GEO);
	shared.reqIndexPop = getRequiredIndex(dataSet, PROPERTY_POP);
	if (shared.reqIndexCountryCode < 0 ||
		shared.reqIndexCountry < 0 ||
		shared.reqIndexConfidence < 0 ||
		shared.reqIndexConnection < 0 ||
		shared.reqIndexGeo < 0 ||
		shared.reqIndexPop < 0) {
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_PROPERTIES_MISSING;
	}
	ResultsIpi* results = ResultsIpiCreate(&manager);
	if (results == NULL) {
		printf("Not enough memory to probe addresses.\n");
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return COUNTRY_OVERLAP_FAILED;
	}
	for (int i = 3; i < argc; i++) {
		probeAddress(&shared, results, argv[i]);
	}
	ResultsIpiFree(results);
	DataSetIpiRelease(dataSet);
	ResourceManagerFree(&manager);
	return COUNTRY_OVERLAP_OK;
}

int main(int argc, char* argv[]) {
	StatusCode status = SUCCESS;
	char dataFilePath[FILE_MAX_PATH];
	dataFilePath[0] = '\0';
	if (argc > 1) {
		// The path is checked against the buffer because a longer
		// argument would otherwise overrun it.
		if (strlen(argv[1]) >= sizeof(dataFilePath)) {
			printf(
				"The data file path is longer than the %d characters "
				"supported.\n",
				(int)sizeof(dataFilePath) - 1);
			return COUNTRY_OVERLAP_FAILED;
		}
		snprintf(dataFilePath, sizeof(dataFilePath), "%s", argv[1]);
	}
	else {
		for (int i = 0;
			i < (int)(sizeof(dataFileNames) / sizeof(dataFileNames[0]));
			i++) {
			status = FileGetPath(
				dataDir,
				dataFileNames[i],
				dataFilePath,
				sizeof(dataFilePath));
			if (status == SUCCESS) {
				break;
			}
		}
		if (status != SUCCESS) {
			countryOverlapReportStatus(status, dataFileNames[0]);
			printf(
				"Provide the path to an enterprise IP Intelligence "
				"data file as the first argument.\n");
			return 1;
		}
	}

	// Probe mode prints the values for the supplied addresses so
	// results can be verified against other 51Degrees APIs.
	// Usage: CountryOverlap <data file> --probe <ip> [ip...]
	// Without any addresses this is an error rather than a sweep, which
	// would otherwise run for an hour and write its CSV to a file named
	// --probe.
	if (argc > 2 && strcmp(argv[2], "--probe") == 0) {
		if (argc < 4) {
			printf(
				"Provide at least one IP address to probe. Usage: "
				"CountryOverlap <data file> --probe <ip> [ip...]\n");
			return COUNTRY_OVERLAP_FAILED;
		}
		return runProbe(dataFilePath, argc, argv);
	}

	const char* outputPath = argc > 2 ? argv[2] :
		"country-overlap.csv";
	const int threadCount = argc > 3 ? atoi(argv[3]) : 10;
	const int totalChunks = argc > 4 ? atoi(argv[4]) : 256;
	const double minSecondaryPercent = argc > 5 ?
		atof(argv[5]) : DEFAULT_MIN_SECONDARY_PERCENT;

	return fiftyoneDegreesIpiCountryOverlap(
		dataFilePath,
		NULL,
		outputPath,
		threadCount,
		0,
		totalChunks,
		minSecondaryPercent);
}

#endif
