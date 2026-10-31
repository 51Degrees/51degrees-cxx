/* Temporary. For one set of properties and one preset, prints what is held in
 * memory, the reads of each collection per lookup, the start-up time, the
 * memory, a digest of every answer and the lookups a second. Not part of any
 * pull request. */
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <time.h>
#else
#include <time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipi.h"
#include "fiftyone.h"
#include "../common/collectionKeyTypes.h"

MAP_TYPE(Collection)

#define BUFFER_SIZE (1024 * 1024)
#define MAX_ADDRESSES 20000
#define MAX_COUNTED 128
#define MAX_THREADS 16

static char addresses[MAX_ADDRESSES][64];
static int addressCount;

#ifdef _MSC_VER
#include <intrin.h>
static volatile __int64 mallocCalls;
#define COUNT_CALL() _InterlockedIncrement64(&mallocCalls)
#else
static volatile long long mallocCalls;
#define COUNT_CALL() __atomic_add_fetch(&mallocCalls, 1, __ATOMIC_SEQ_CST)
#endif

static void *countingMalloc(size_t size) {
	COUNT_CALL();
	return malloc(size);
}

static void *countingMallocAligned(int alignment, size_t size) {
	COUNT_CALL();
	return fiftyoneDegreesMemoryStandardMallocAligned(alignment, size);
}

static uint64_t fnv(uint64_t hash, const void *data, size_t length) {
	const unsigned char *bytes = (const unsigned char *)data;
	for (size_t i = 0; i < length; i++) {
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

/* Resident memory in millions of bytes. */
static double residentMb(void) {
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS counters;
	if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
		return (double)counters.WorkingSetSize / 1000000.0;
	}
	return -1;
#elif defined(__APPLE__)
	mach_task_basic_info_data_t info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
		(task_info_t)&info, &count) == KERN_SUCCESS) {
		return (double)info.resident_size / 1000000.0;
	}
	return -1;
#else
	char line[256];
	double value = -1;
	FILE *file = fopen("/proc/self/status", "r");
	if (file == NULL) return -1;
	while (fgets(line, sizeof(line), file) != NULL) {
		if (strncmp(line, "VmRSS:", 6) == 0) {
			value = atof(line + 6) * 1024.0 / 1000000.0;
		}
	}
	fclose(file);
	return value;
#endif
}

static double now(void) {
#ifdef _WIN32
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
#endif
}

/* The addresses from the file in the same mixed order every run. */
static void loadAddresses(const char *path) {
	char line[128];
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		fprintf(stderr, "no address file\n");
		exit(2);
	}
	while (addressCount < MAX_ADDRESSES && fgets(line, sizeof(line), file) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		if (strchr(line, '.') != NULL || strchr(line, ':') != NULL) {
			strncpy(addresses[addressCount], line, 63);
			addresses[addressCount++][63] = '\0';
		}
	}
	fclose(file);
	uint64_t state = 0x9E3779B97F4A7C15ULL;
	char swap[64];
	for (int i = addressCount - 1; i > 0; i--) {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		const int j = (int)(state % (uint64_t)(i + 1));
		memcpy(swap, addresses[i], 64);
		memcpy(addresses[i], addresses[j], 64);
		memcpy(addresses[j], swap, 64);
	}
}

static uint64_t nextRandom(uint64_t *state) {
	*state ^= *state << 13;
	*state ^= *state >> 7;
	*state ^= *state << 17;
	return *state;
}

/* Counts the reads of each collection during one pass on one thread. */
typedef struct {
	const char *name;
	Collection *collection;
	fiftyoneDegreesCollectionGetMethod original;
	unsigned long long gets;
	bool isMiss;
} Counted;

static Counted counted[MAX_COUNTED];
static int countedCount;

static void *countingGet(
	const Collection *collection,
	const CollectionKey *key,
	Item *item,
	Exception *exception) {
	for (int i = 0; i < countedCount; i++) {
		if (counted[i].collection == collection) {
			counted[i].gets++;
			return counted[i].original(collection, key, item, exception);
		}
	}
	return NULL;
}

static void countOne(const char *name, Collection *collection, bool isMiss) {
	if (countedCount >= MAX_COUNTED) return;
	counted[countedCount].name = name;
	counted[countedCount].collection = collection;
	counted[countedCount].original = collection->get;
	counted[countedCount].gets = 0;
	counted[countedCount].isMiss = isMiss;
	countedCount++;
	collection->get = countingGet;
}

/* A held collection keeps the collection it reads from in its first field. */
static void countCollection(const char *name, Collection *collection) {
	if (strcmp(collection->typeName, "CollectionCache") == 0) {
		fiftyoneDegreesCollectionCache *cache =
			(fiftyoneDegreesCollectionCache *)collection->state;
		countOne(name, cache->source, true);
	}
	else if (strcmp(collection->typeName, "CollectionHeld") == 0) {
		Collection *source = *(Collection **)collection->state;
		if (strcmp(source->typeName, "CollectionCache") == 0) {
			fiftyoneDegreesCollectionCache *cache =
				(fiftyoneDegreesCollectionCache *)source->state;
			countOne(name, cache->source, true);
		}
		else {
			countOne(name, source, true);
		}
	}
	countOne(name, collection, false);
}

static void countStart(DataSetIpi *dataSet) {
	countCollection("strings", dataSet->strings);
	countCollection("components", dataSet->components);
	countCollection("maps", dataSet->maps);
	countCollection("properties", dataSet->properties);
	countCollection("values", dataSet->values);
	countCollection("profiles", dataSet->profiles);
	countCollection("graphs", dataSet->graphs);
	countCollection("profileGroups", dataSet->profileGroups);
	countCollection("propertyTypes", dataSet->propertyTypes);
	countCollection("profileOffsets", dataSet->profileOffsets);
	for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
		countCollection("graphNodes", dataSet->graphsArray->items[g].nodes);
		countCollection("graphSpans", dataSet->graphsArray->items[g].spans);
		countCollection("graphSpanBytes", dataSet->graphsArray->items[g].spanBytes);
		countCollection("graphClusters", dataSet->graphsArray->items[g].clusters);
	}
}

static void countStop(int lookups) {
	static const char *names[] = {
		"strings", "components", "maps", "properties", "values", "profiles",
		"graphs", "profileGroups", "propertyTypes", "profileOffsets",
		"graphNodes", "graphSpans", "graphSpanBytes", "graphClusters" };
	for (int i = 0; i < countedCount; i++) {
		counted[i].collection->get = counted[i].original;
	}
	printf("READS");
	for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
		unsigned long long gets = 0, misses = 0;
		for (int i = 0; i < countedCount; i++) {
			if (strcmp(counted[i].name, names[n]) == 0) {
				if (counted[i].isMiss) misses += counted[i].gets;
				else gets += counted[i].gets;
			}
		}
		if (gets > 0) {
			printf(" %s=%.1f/%.1f", names[n],
				(double)gets / lookups, (double)misses / lookups);
		}
	}
	printf("\n");
}

/* Millions of bytes in memory, reading the held bytes after the source. */
static double held(Collection *collection, int shift) {
	if (strcmp(collection->typeName, "CollectionMemory") == 0) {
		return (double)((uint64_t)collection->size << shift) / 1000000.0;
	}
	if (strcmp(collection->typeName, "CollectionHeld") == 0) {
		return (double)((uint64_t *)collection->state)[1] / 1000000.0;
	}
	return 0;
}

static void printHeld(DataSetIpi *dataSet) {
	const int shift = dataSet->header.profilesOffsetShift;
	double graphMb = 0;
	for (uint32_t g = 0; g < dataSet->graphsArray->count; g++) {
		graphMb += held(dataSet->graphsArray->items[g].nodes, 0);
		graphMb += held(dataSet->graphsArray->items[g].spans, 0);
		graphMb += held(dataSet->graphsArray->items[g].spanBytes, 0);
		graphMb += held(dataSet->graphsArray->items[g].clusters, 0);
	}
	printf("HELD strings=%s:%.1f values=%s:%.1f profiles=%s:%.1f "
		"propertyTypes=%s:%.3f profileOffsets=%s:%.1f graphData=%.1f\n",
		dataSet->strings->typeName + 10, held(dataSet->strings, 0),
		dataSet->values->typeName + 10, held(dataSet->values, 0),
		dataSet->profiles->typeName + 10, held(dataSet->profiles, shift),
		dataSet->propertyTypes->typeName + 10, held(dataSet->propertyTypes, 0),
		dataSet->profileOffsets->typeName + 10, held(dataSet->profileOffsets, 0),
		graphMb);
}

typedef struct {
	ResourceManager *manager;
	int start;
	bool uniform;
	double seconds;
	unsigned long long done;
	unsigned long long failures;
} Work;

/* Finds the result and reads every required property as text. */
static void timedLookups(void *arg) {
	Work *work = (Work *)arg;
	char *buffer = (char *)malloc(BUFFER_SIZE);
	ResultsIpi *results = ResultsIpiCreate(work->manager);
	DataSetIpi *dataSet = (DataSetIpi *)results->b.dataSet;
	const int count = (int)dataSet->b.b.available->count;
	const double stop = now() + work->seconds;
	int index = work->start;
	uint64_t random = 0x9E3779B97F4A7C15ULL + (uint64_t)work->start;
	while (true) {
		if ((work->done & 63) == 0 && now() >= stop) break;
		EXCEPTION_CREATE;
		if (work->uniform) {
			const uint64_t next = nextRandom(&random);
			ResultsIpiFromIpAddress(
				results, (const unsigned char *)&next, 4, IP_TYPE_IPV4, exception);
		}
		else {
			const char *address = addresses[index];
			index = (index + 1) % addressCount;
			ResultsIpiFromIpAddressString(results, address, strlen(address), exception);
		}
		if (EXCEPTION_FAILED) {
			work->failures++;
			continue;
		}
		for (int p = 0; p < count; p++) {
			ResultsIpiGetValuesStringByRequiredPropertyIndex(
				results, p, buffer, BUFFER_SIZE, "|", exception);
		}
		if (EXCEPTION_FAILED) work->failures++;
		work->done++;
	}
	ResultsIpiFree(results);
	free(buffer);
	THREAD_EXIT;
}

static double timed(
	ResourceManager *manager,
	int threads,
	double seconds,
	bool uniform,
	double *allocations) {
	Work works[MAX_THREADS];
	FIFTYONE_DEGREES_THREAD handles[MAX_THREADS];
	const long long before = mallocCalls;
	const double started = now();
	for (int t = 0; t < threads; t++) {
		works[t].manager = manager;
		works[t].start = t * (addressCount / threads);
		works[t].uniform = uniform;
		works[t].seconds = seconds;
		works[t].done = 0;
		works[t].failures = 0;
		THREAD_CREATE(handles[t], (THREAD_ROUTINE)&timedLookups, &works[t]);
	}
	unsigned long long done = 0, failures = 0;
	for (int t = 0; t < threads; t++) {
		THREAD_JOIN(handles[t]);
		THREAD_CLOSE(handles[t]);
		done += works[t].done;
		failures += works[t].failures;
	}
	const double elapsed = now() - started;
	*allocations = done > 0 ? (double)(mallocCalls - before) / (double)done : 0;
	if (failures > 0) printf("FAILURES %llu\n", failures);
	return (double)done / elapsed;
}

/* Hashes everything a caller can read for the result in the results. */
static uint64_t digestResult(
	ResultsIpi *results,
	int count,
	char *buffer,
	uint64_t digest,
	unsigned long long *failures) {
	EXCEPTION_CREATE;
	for (int p = 0; p < count; p++) {
		const bool has = ResultsIpiGetHasValues(results, p, exception);
		digest = fnv(digest, &has, sizeof(has));
		const size_t added = ResultsIpiGetValuesStringByRequiredPropertyIndex(
			results, p, buffer, BUFFER_SIZE, "|", exception);
		digest = fnv(digest, &added, sizeof(added));
		digest = fnv(digest, buffer, added < BUFFER_SIZE ? added : BUFFER_SIZE);
		if (EXCEPTION_FAILED) {
			(*failures)++;
			EXCEPTION_CLEAR;
		}
	}
	return digest;
}

/* What part of the strings the set really reads. */
typedef struct {
	uint32_t first;
	uint32_t end;
} Span;

static int compareSpans(const void *a, const void *b) {
	const uint32_t x = ((const Span *)a)->first, y = ((const Span *)b)->first;
	return x < y ? -1 : (x > y ? 1 : 0);
}

static void addSpan(
	DataSetIpi *dataSet,
	Span *spans,
	uint64_t *count,
	uint32_t valueIndex,
	PropertyValueType type,
	Exception *exception) {
	Item item;
	DataReset(&item.data);
	const CollectionKey key = { valueIndex, CollectionKeyType_Value };
	const Value *value = (const Value *)dataSet->values->get(
		dataSet->values, &key, &item, exception);
	const uint32_t offset = (uint32_t)value->nameOffset;
	COLLECTION_RELEASE(dataSet->values, &item);
	DataReset(&item.data);
	const CollectionKeyType *keyType =
		GetCollectionKeyTypeForStoredValueType(type, exception);
	const void *stored = StoredBinaryValueGet(
		dataSet->strings, offset, type, &item, exception);
	const uint32_t size = keyType->getFinalSizeMethod != NULL ?
		keyType->getFinalSizeMethod(stored, exception) :
		keyType->initialBytesCount;
	COLLECTION_RELEASE(dataSet->strings, &item);
	spans[*count].first = offset;
	spans[*count].end = offset + size;
	(*count)++;
}

static int gaps(const char *fileName, const char *list) {
	ConfigIpi config = IpiHighPerformanceConfig;
	PropertiesRequired required = PropertiesDefault;
	required.string = list;
	ResourceManager manager;
	EXCEPTION_CREATE;
	if (IpiInitManagerFromFile(&manager, &config, &required, fileName, exception)
		!= SUCCESS || EXCEPTION_FAILED) {
		printf("GAPS init failed\n");
		return 1;
	}
	DataSetIpi *dataSet = DataSetIpiGet(&manager);
	const uint64_t capacity = (uint64_t)dataSet->header.values.count + 64;
	Span *spans = (Span *)malloc(capacity * sizeof(Span));
	uint64_t count = 0;
	for (uint32_t i = 0; i < dataSet->b.b.available->count; i++) {
		Item item;
		DataReset(&item.data);
		const Property *property = PropertyGet(
			dataSet->properties,
			dataSet->b.b.available->items[i].propertyIndex,
			&item, exception);
		const PropertyValueType type = PropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			dataSet->b.b.available->items[i].propertyIndex,
			exception);
		for (uint32_t v = property->firstValueIndex;
			v <= property->lastValueIndex && count < capacity - 1;
			v++) {
			addSpan(dataSet, spans, &count, v, type, exception);
		}
		if (property->defaultValueIndex != UINT32_MAX) {
			const bool inside =
				property->defaultValueIndex >= property->firstValueIndex &&
				property->defaultValueIndex <= property->lastValueIndex;
			printf("DEFAULT property=%s index=%u first=%u last=%u inside=%d\n",
				(const char *)&((const String *)
					dataSet->b.b.available->items[i].name.data.ptr)->value,
				property->defaultValueIndex,
				property->firstValueIndex,
				property->lastValueIndex,
				inside ? 1 : 0);
			addSpan(dataSet, spans, &count, property->defaultValueIndex, type, exception);
		}
		COLLECTION_RELEASE(dataSet->properties, &item);
	}
	qsort(spans, (size_t)count, sizeof(Span), compareSpans);
	const uint32_t size = dataSet->strings->size;
	static const uint32_t thresholds[] = {
		4096, 16384, 65536, 262144, 1048576, 4194304, 16777216 };
	for (size_t t = 0; t < sizeof(thresholds) / sizeof(thresholds[0]); t++) {
		uint64_t leftOut = 0;
		uint64_t ranges = 0;
		uint32_t end = 0;
		for (uint64_t s = 0; s < count; s++) {
			if (spans[s].first > end && spans[s].first - end >= thresholds[t]) {
				leftOut += spans[s].first - end;
				ranges++;
			}
			if (spans[s].end > end) end = spans[s].end;
		}
		if (size > end && size - end >= thresholds[t]) {
			leftOut += size - end;
		}
		printf("GAPS gapAtLeast=%u leftOutMb=%.1f gaps=%llu\n",
			thresholds[t], (double)leftOut / 1000000.0, (unsigned long long)ranges);
	}
	uint64_t needed = 0;
	uint32_t end = 0;
	for (uint64_t s = 0; s < count; s++) {
		const uint32_t from = spans[s].first > end ? spans[s].first : end;
		if (spans[s].end > from) {
			needed += spans[s].end - from;
			end = spans[s].end;
		}
	}
	printf("GAPS stringsMb=%.1f neededMb=%.1f values=%llu\n",
		(double)size / 1000000.0, (double)needed / 1000000.0,
		(unsigned long long)count);
	free(spans);
	DataSetIpiRelease(dataSet);
	ResourceManagerFree(&manager);
	return 0;
}

static size_t optionNumber(const char *options, const char *name, size_t otherwise) {
	const char *at = strstr(options, name);
	return at == NULL ? otherwise : (size_t)strtoull(at + strlen(name), NULL, 10);
}

int main(int argc, char **argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc >= 4 && strcmp(argv[1], "gaps") == 0) {
		return gaps(argv[2], argv[3]);
	}
	if (argc >= 3 && strcmp(argv[1], "names") == 0) {
		ConfigIpi lowMemory = IpiLowMemoryConfig;
		ResourceManager manager;
		EXCEPTION_CREATE;
		if (IpiInitManagerFromFile(&manager, &lowMemory, NULL, argv[2], exception)
			!= SUCCESS || EXCEPTION_FAILED) {
			return 1;
		}
		DataSetIpi *dataSet = DataSetIpiGet(&manager);
		for (uint32_t i = 0; i < dataSet->b.b.available->count; i++) {
			printf("%s%s", i == 0 ? "" : ",", FIFTYONE_DEGREES_STRING(
				dataSet->b.b.available->items[i].name.data.ptr));
		}
		printf("\n");
		DataSetIpiRelease(dataSet);
		ResourceManagerFree(&manager);
		return 0;
	}
	if (argc < 6) {
		fprintf(stderr, "usage: file addresses preset required options\n");
		return 2;
	}
	const char *fileName = argv[1];
	loadAddresses(argv[2]);
	const char *configName = argv[3];
	const char *requiredList = argv[4];
	const char *options = argv[5];
	const double seconds = (double)optionNumber(options, "seconds=", 2);

	ConfigIpi config;
	if (strcmp(configName, "balanced") == 0) config = IpiBalancedConfig;
	else if (strcmp(configName, "temp") == 0) config = IpiBalancedTempConfig;
	else if (strcmp(configName, "high") == 0) config = IpiHighPerformanceConfig;
	else if (strcmp(configName, "low") == 0) config = IpiLowMemoryConfig;
	else if (strcmp(configName, "memory") == 0) config = IpiInMemoryConfig;
	else return 2;
	static const fiftyoneDegreesCollectionConfig inMemory = {
		true, 0, FIFTYONE_DEGREES_CACHE_CONCURRENCY };
	static const fiftyoneDegreesCollectionConfig cachedOffsets = {
		false, 10000, FIFTYONE_DEGREES_CACHE_CONCURRENCY };
	if (strstr(options, "types") != NULL) {
		config.propertyTypes = inMemory;
		config.profileOffsets = inMemory;
	}
	if (strstr(options, "literal") != NULL) {
		config.propertyTypes = inMemory;
		config.profileOffsets = cachedOffsets;
	}
	if (strstr(options, "heldvalues") != NULL) config.values.loaded = true;
	if (strstr(options, "heldprofiles") != NULL) config.profiles.loaded = true;
	config.profiles.capacity = (uint32_t)optionNumber(
		options, "cache=", config.profiles.capacity);

	PropertiesRequired required = PropertiesDefault;
	if (strcmp(requiredList, "-") != 0) {
		required.string = requiredList;
	}

	ResourceManager manager;
	EXCEPTION_CREATE;
	const double sizeMb = strstr(options, "nosize") != NULL ? 0 :
		(double)fiftyoneDegreesIpiSizeManagerFromFile(&config, &required, fileName, exception) /
		1000000.0;
	if (EXCEPTION_FAILED) {
		printf("SIZE FAILED exception=%d\n", (int)exception->status);
		return 1;
	}

	fiftyoneDegreesMalloc = countingMalloc;
	fiftyoneDegreesMallocAligned = countingMallocAligned;
	const double initStarted = now();
	StatusCode status = IpiInitManagerFromFile(
		&manager, &config, &required, fileName, exception);
	const double initSeconds = now() - initStarted;
	if (status != SUCCESS || EXCEPTION_FAILED) {
		printf("INIT FAILED status=%d exception=%d\n",
			(int)status, (int)exception->status);
		return 1;
	}
	const double initMb = residentMb();
	DataSetIpi *dataSet = DataSetIpiGet(&manager);
	printHeld(dataSet);
	const int count = (int)dataSet->b.b.available->count;

	/* One pass over every address, counting the reads and hashing the answers. */
	countStart(dataSet);
	char *buffer = (char *)malloc(BUFFER_SIZE);
	ResultsIpi *results = ResultsIpiCreate(&manager);
	uint64_t digest = 14695981039346656037ULL;
	unsigned long long failures = 0;
	const long long callsBefore = mallocCalls;
	for (int i = 0; i < addressCount; i++) {
		EXCEPTION_CLEAR;
		ResultsIpiFromIpAddressString(results, addresses[i], strlen(addresses[i]), exception);
		if (EXCEPTION_FAILED) {
			failures++;
			continue;
		}
		digest = digestResult(results, count, buffer, digest, &failures);
	}
	const double passAllocations =
		(double)(mallocCalls - callsBefore) / addressCount;
	countStop(addressCount);

	/* Random addresses of both versions, which reach profiles the file of
	 * addresses does not, then a lookup for one property and for none, where
	 * every other property reads as no value or as its default. */
	uint64_t random = 0x2545F4914F6CDD1DULL;
	for (int i = 0; i < 10000; i++) {
		unsigned char ip[16];
		for (int b = 0; b < 16; b += 8) {
			const uint64_t next = nextRandom(&random);
			memcpy(ip + b, &next, 8);
		}
		EXCEPTION_CLEAR;
		if (i % 2 == 0) {
			ResultsIpiFromIpAddress(results, ip, 4, IP_TYPE_IPV4, exception);
		}
		else {
			/* A global address, most of the rest being empty space. */
			ip[0] = (unsigned char)(0x20 | (ip[0] & 0x0F));
			ResultsIpiFromIpAddress(results, ip, 16, IP_TYPE_IPV6, exception);
		}
		if (EXCEPTION_FAILED) {
			failures++;
			continue;
		}
		digest = digestResult(results, count, buffer, digest, &failures);
	}
	for (int i = 0; i < 2000 && i < addressCount; i++) {
		int only = i % count;
		EXCEPTION_CLEAR;
		ResultsIpiFromIpAddressStringForProperties(
			results, addresses[i], strlen(addresses[i]), &only,
			(i % 4 == 0) ? 0 : 1, exception);
		if (EXCEPTION_FAILED) {
			failures++;
			continue;
		}
		digest = digestResult(results, count, buffer, digest, &failures);
	}
	ResultsIpiFree(results);
	free(buffer);
	DataSetIpiRelease(dataSet);

	double allocations1, allocations4, allocationsUniform;
	const double rate1 = timed(&manager, 1, seconds, false, &allocations1);
	const double rate4 = timed(&manager, 4, seconds, false, &allocations4);
	const double uniform1 = timed(&manager, 1, seconds, true, &allocationsUniform);
	const double uniform4 = timed(&manager, 4, seconds, true, &allocationsUniform);
	printf("CASE required=%d sizeMb=%.1f initSeconds=%.2f residentAtInitMb=%.0f "
		"residentMb=%.0f oneThread=%.0f fourThreads=%.0f uniformOne=%.0f "
		"uniformFour=%.0f allocFirstPass=%.2f allocOneThread=%.2f allocUniform=%.2f "
		"failures=%llu digest=%016llx\n",
		count, sizeMb, initSeconds, initMb, residentMb(), rate1, rate4,
		uniform1, uniform4, passAllocations, allocations1, allocationsUniform,
		failures, (unsigned long long)digest);
	ResourceManagerFree(&manager);
	return 0;
}
