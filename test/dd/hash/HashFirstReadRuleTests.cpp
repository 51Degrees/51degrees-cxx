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
 * @file HashFirstReadRuleTests.cpp
 * @brief Tests when Hash results walk a component's graph. With no properties
 * named when the data set was created, populating the results walks no
 * graph. With properties named, populating walks the graph of each named
 * property's component and no other. Either way, a read walks its property's
 * component only if it has not been walked. The tests name properties with
 * the C properties string and array and with the C++ builder, and cover a
 * graph that cannot be read, every allocation failing in turn, evidence that
 * answers no component, properties that are not available, and reloads.
 *
 * A walk sets the value it walked in the result it walks into, so a result
 * whose target value is set has been walked, whether or not the walk went
 * through the method the results call to walk a component.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/hash.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

const char iPhoneUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53";

const char chromeUserAgent[] =
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	"(KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36";

/** A property of the hardware component that every Hash data file has. */
const char hardwareProperty[] = "IsMobile";

/** A property of the browser component that every Hash data file has. */
const char browserProperty[] = "BrowserName";

/** A property whose value evidence can override. */
const char overriddenName[] = "ScreenPixelsWidth";

/** The value the overriding evidence gives it. */
const char overriddenValue[] = "12345";

/** Bytes for the text of a property's values. */
const size_t valueBufferBytes = 1 << 16;

/** Threads that use their own results while the data set is reloaded. */
const int readerThreads = 4;

/** Reloads made while the threads use their own results. */
const int reloadsWhileReading = 12;

/** The components walked, in order, by the results that count their walks. */
std::vector<int>* walkLog = nullptr;

/** The walk the results make when they are not counting, or null. */
fiftyoneDegreesResultsEvaluateMethod walkMethod = nullptr;

/** Records the component in the log, then walks it if the results walk. */
void countWalk(
	fiftyoneDegreesResultsBase* results,
	unsigned char componentIndex,
	fiftyoneDegreesException* exception) {
	if (walkLog != nullptr) {
		walkLog->push_back((int)componentIndex);
	}
	if (walkMethod != nullptr) {
		walkMethod(results, componentIndex, exception);
	}
}

/** Makes the results record each walk they make through their method. */
void countWalks(fiftyoneDegreesResultsHash* results, std::vector<int>& log) {
	walkMethod = results->b.b.evaluate;
	walkLog = &log;
	results->b.b.evaluate = countWalk;
}

/** The allocations made so far by countingMalloc. */
int mallocCalls = 0;

/** The allocation that fails, counting from one, or zero for none. */
int mallocFailsOn = 0;

void* FIFTYONE_DEGREES_CALL_CONV countingMalloc(size_t size) {
	return ++mallocCalls == mallocFailsOn ?
		nullptr : fiftyoneDegreesMemoryTrackingMalloc(size);
}

/** Counts each allocation, failing the one given, until stopCounting. */
void startCounting(int failsOn) {
	mallocCalls = 0;
	mallocFailsOn = failsOn;
	fiftyoneDegreesMalloc = countingMalloc;
}

/** Stops counting, leaving the tracking allocation in place. */
void stopCounting() {
	fiftyoneDegreesMalloc = fiftyoneDegreesMemoryTrackingMalloc;
}

/**
 * Tracks every allocation for as long as it lasts. What is allocated while it
 * lasts must be freed while it lasts, as the tracking free cannot free what
 * the tracking allocation did not make, and a standard free leaves the
 * tracking record behind.
 */
class Tracking {
public:
	Tracking() {
		fiftyoneDegreesSetUpMemoryTracking();
	}
	~Tracking() {
		fiftyoneDegreesUnsetMemoryTracking();
	}
	Tracking(const Tracking&) = delete;
	Tracking& operator=(const Tracking&) = delete;
};

/** Results that are freed on every way out of a test. */
typedef std::unique_ptr<
	fiftyoneDegreesResultsHash,
	decltype(&fiftyoneDegreesResultsHashFree)> OwnedResults;

fiftyoneDegreesDataSetHash* dataSetOf(fiftyoneDegreesResultsHash* results) {
	return (fiftyoneDegreesDataSetHash*)results->b.b.dataSet;
}

/** For each result, true if a walk has set the value it walked. */
std::vector<bool> walkedResults(fiftyoneDegreesResultsHash* results) {
	std::vector<bool> walked;
	for (uint32_t i = 0; i < results->count; i++) {
		walked.push_back(results->items[i].b.targetUserAgent != nullptr);
	}
	return walked;
}

/** True if no result has been walked. */
bool noResultWalked(fiftyoneDegreesResultsHash* results) {
	const std::vector<bool> walked = walkedResults(results);
	return std::find(walked.begin(), walked.end(), true) == walked.end();
}

/** True if every result has been walked. */
bool everyResultWalked(fiftyoneDegreesResultsHash* results) {
	const std::vector<bool> walked = walkedResults(results);
	return std::find(walked.begin(), walked.end(), false) == walked.end();
}

/** The available components of the results' data set, in order. */
std::vector<int> availableComponents(fiftyoneDegreesResultsHash* results) {
	fiftyoneDegreesDataSetHash* dataSet = dataSetOf(results);
	std::vector<int> components;
	for (uint32_t i = 0; i < dataSet->componentsList.count; i++) {
		if (dataSet->componentsAvailable[i]) {
			components.push_back((int)i);
		}
	}
	return components;
}

/** The component of the available property named, or -1. */
int componentOfName(fiftyoneDegreesResultsHash* results, const char* name) {
	fiftyoneDegreesPropertiesAvailable* available =
		dataSetOf(results)->b.b.available;
	for (uint32_t i = 0; i < available->count; i++) {
		if (strcmp(name, FIFTYONE_DEGREES_STRING(
			available->items[i].name.data.ptr)) == 0) {
			return (int)available->items[i].componentIndex;
		}
	}
	return -1;
}

/** The values of the property named, or nothing if it has none. */
std::string valueOf(fiftyoneDegreesResultsHash* results, const char* name) {
	std::vector<char> buffer(valueBufferBytes, '\0');
	char separator[] = "|";
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashGetValuesString(
		results,
		name,
		buffer.data(),
		buffer.size(),
		separator,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(buffer.data());
}

/** The values of the property at the position, or why it has none. */
std::string describe(fiftyoneDegreesResultsHash* results, int position) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	if (fiftyoneDegreesResultsHashGetHasValues(
		results,
		position,
		exception) == false) {
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		const int reason = (int)fiftyoneDegreesResultsHashGetNoValueReason(
			results,
			position,
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		return "no value, reason " + std::to_string(reason);
	}
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	std::vector<char> buffer(valueBufferBytes, '\0');
	char separator[] = "|";
	fiftyoneDegreesResultsHashGetValuesStringByRequiredPropertyIndex(
		results,
		position,
		buffer.data(),
		buffer.size(),
		separator,
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(buffer.data());
}

/** Every position of the names given, or of the properties when none. */
int positionCount(fiftyoneDegreesResultsHash* results) {
	return (int)fiftyoneDegreesPropertiesGetRequiredPropertyCount(
		dataSetOf(results)->b.b.available);
}

/** What every position reads, in order. */
std::vector<std::string> describeAll(fiftyoneDegreesResultsHash* results) {
	std::vector<std::string> values;
	for (int i = 0; i < positionCount(results); i++) {
		values.push_back(describe(results, i));
	}
	return values;
}

/** The status and the text a read of the position gives. */
std::string readAt(
	fiftyoneDegreesResultsHash* results,
	int position,
	fiftyoneDegreesException* exception) {
	std::vector<char> buffer(valueBufferBytes, '\0');
	char separator[] = "|";
	fiftyoneDegreesResultsHashGetValuesStringByRequiredPropertyIndex(
		results,
		position,
		buffer.data(),
		buffer.size(),
		separator,
		exception);
	return std::to_string((int)exception->status) + " " +
		std::string(buffer.data());
}

/** The pairs of some evidence, as prefixes, keys and values. */
typedef std::vector<std::pair<
	fiftyoneDegreesEvidencePrefix,
	std::pair<std::string, std::string>>> EvidencePairs;

/**
 * Evidence made of the pairs given. It keeps its own copies of the keys and
 * values, and a copy makes evidence of its own.
 */
class Evidence {
public:
	explicit Evidence(const EvidencePairs& given) : pairs(given) {
		build();
	}
	Evidence(const Evidence& other) : pairs(other.pairs) {
		build();
	}
	Evidence& operator=(const Evidence&) = delete;
	~Evidence() {
		fiftyoneDegreesEvidenceFree(array);
	}
	fiftyoneDegreesEvidenceKeyValuePairArray* array = nullptr;
private:
	void build() {
		array = fiftyoneDegreesEvidenceCreate(
			(uint32_t)std::max<size_t>(1, pairs.size()));
		for (const auto& pair : pairs) {
			fiftyoneDegreesEvidenceAddString(
				array,
				pair.first,
				pair.second.first.c_str(),
				pair.second.second.c_str());
		}
	}
	EvidencePairs pairs;
};

/** Evidence of a User-Agent on its own. */
Evidence userAgent(const char* value) {
	return Evidence(EvidencePairs({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", value } } }));
}

} // namespace

/**
 * Tests through the C API on the first Hash data file found, and through the
 * C++ classes, whose results the tests read as a friend of the class. A skip
 * in SetUp does not stop the test with the Visual Studio googletest, so each
 * test first checks that SetUp found a data file.
 */
class HashFirstReadRuleTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _HashFileNamesLength && dataFilePath.empty();
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No Hash data file was found.";
		}
	}

	void TearDown() override {
		walkLog = nullptr;
		walkMethod = nullptr;
		removeCopy();
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::string().swap(dataFilePath);
		std::string().swap(copyPath);
		std::vector<unsigned char>().swap(good);
		std::vector<unsigned char>().swap(renamed);
		std::string().swap(renamedName);
		Base::TearDown();
	}

protected:
	std::string dataFilePath;
	std::string copyPath;
	std::vector<unsigned char> good;
	std::vector<unsigned char> renamed;
	std::string renamedName;

	/** A manager for the data file, freed when it goes. */
	class Manager {
	public:
		~Manager() {
			release();
		}
		/** Frees the manager, if it was initialised. */
		void release() {
			if (ready) {
				fiftyoneDegreesResourceManagerFree(&manager);
				ready = false;
			}
		}
		fiftyoneDegreesResourceManager manager;
		bool ready = false;
	};

	/** Initialises the manager from the file, naming the properties given. */
	void init(
		Manager& m,
		fiftyoneDegreesPropertiesRequired properties,
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashBalancedConfig,
		const std::string& path = std::string()) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				&m.manager,
				&config,
				&properties,
				path.empty() ? dataFilePath.c_str() : path.c_str(),
				exception));
		m.ready = true;
	}

	/** Properties naming those in the string, or every one if null. */
	static fiftyoneDegreesPropertiesRequired named(const char* names) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = names;
		return properties;
	}

	/** Populates the results from the evidence, failing on an error. */
	static void populate(
		fiftyoneDegreesResultsHash* results,
		Evidence evidence) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromEvidence(
			results,
			evidence.array,
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
	}

	/** The C results a C++ results instance holds. */
	static fiftyoneDegreesResultsHash* cResultsOf(ResultsHash* results) {
		return results->results;
	}

	/** Names a property of each component of the data file. */
	std::string onePropertyOfEachComponent() {
		Manager all;
		init(all, fiftyoneDegreesPropertiesDefault);
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&all.manager, 0, 0);
		fiftyoneDegreesPropertiesAvailable* available =
			dataSetOf(results)->b.b.available;
		std::vector<int> components;
		std::string names;
		for (uint32_t i = 0; i < available->count; i++) {
			const int component = (int)available->items[i].componentIndex;
			if (std::find(components.begin(), components.end(), component) ==
				components.end()) {
				components.push_back(component);
				names += (names.empty() ? "" : ",") + std::string(
					FIFTYONE_DEGREES_STRING(available->items[i].name.data.ptr));
			}
		}
		fiftyoneDegreesResultsHashFree(results);
		return names;
	}

	/** Reads the data file into good. */
	void readDataFile() {
		FILE* file = fopen(dataFilePath.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << dataFilePath << "'.";
		fseek(file, 0, SEEK_END);
		good.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(good.data(), 1, good.size(), file);
		fclose(file);
		ASSERT_EQ(good.size(), bytes);
	}

	/**
	 * Copies the data file with the first property in name order renamed so
	 * that it sorts after every other name. The name is changed in place, so
	 * the layout of the file is the same, and a data set loaded from the copy
	 * lacks the property under its old name.
	 */
	void renameFirstProperty() {
		ASSERT_NO_FATAL_FAILURE(readDataFile());
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				good.data(),
				(long)good.size(),
				exception));
		fiftyoneDegreesDataSetHash* dataSet =
			fiftyoneDegreesDataSetHashGet(&manager);
		const fiftyoneDegreesPropertyAvailable* first =
			&dataSet->b.b.available->items[0];
		renamedName = FIFTYONE_DEGREES_STRING(first->name.data.ptr);
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			first->propertyIndex,
			&item,
			exception);
		const bool found =
			property != nullptr && FIFTYONE_DEGREES_EXCEPTION_OKAY;
		// The first character of a stored string follows its size.
		const size_t position = found ?
			(size_t)dataSet->header.strings.startPosition +
				property->nameOffset + sizeof(int16_t) :
			0;
		if (found) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		}
		fiftyoneDegreesDataSetHashRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);
		ASSERT_TRUE(found) << "The first property was not read.";
		ASSERT_LT(position, good.size());
		ASSERT_EQ(renamedName[0], (char)good[position]) <<
			"The name of the first property is not where the header and the "
			"property say it is.";
		renamed = good;
		// Sorts after every letter, and no property name has it.
		renamed[position] = '~';
	}

	/** Initialises the manager from memory, naming the properties given. */
	static void initFromMemory(
		Manager& m,
		std::vector<unsigned char>& data,
		fiftyoneDegreesPropertiesRequired properties) {
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromMemory(
				&m.manager,
				&config,
				&properties,
				data.data(),
				(long)data.size(),
				exception));
		m.ready = true;
	}

	/** Reloads the manager from the memory given. */
	static void reload(Manager& m, std::vector<unsigned char>& data) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashReloadManagerFromMemory(
				&m.manager,
				data.data(),
				(long)data.size(),
				exception));
	}

	/**
	 * Copies the data file and returns a manager for the copy whose graphs
	 * are read from the file, with every other collection in memory, then
	 * cuts the copy short where the first graph begins, so that every graph
	 * read fails while everything else can still be read.
	 */
	void initWithGraphsCut(
		Manager& m,
		fiftyoneDegreesPropertiesRequired properties) {
		copyPath = std::string("hash-first-read-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".hash";
		std::filesystem::copy_file(
			dataFilePath,
			copyPath,
			std::filesystem::copy_options::overwrite_existing);
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
		config.strings.loaded = true;
		config.components.loaded = true;
		config.maps.loaded = true;
		config.properties.loaded = true;
		config.values.loaded = true;
		config.profiles.loaded = true;
		config.profileOffsets.loaded = true;
		ASSERT_NO_FATAL_FAILURE(init(m, properties, config, copyPath));
		fiftyoneDegreesDataSetHash* dataSet =
			fiftyoneDegreesDataSetHashGet(&m.manager);
		const uint64_t cut = (std::min)(
			(uint64_t)dataSet->header.rootNodes.startPosition,
			(uint64_t)dataSet->header.nodes.startPosition);
		fiftyoneDegreesDataSetHashRelease(dataSet);
		std::filesystem::resize_file(copyPath, cut);
	}

	/** Removes the copy of the data file, if one was made. */
	void removeCopy() {
		if (copyPath.empty() == false) {
			std::error_code error;
			std::filesystem::remove(copyPath, error);
		}
	}

	/**
	 * Makes each allocation of creating the results, populating them and
	 * reading every position fail in turn. Each failure must be reported, by
	 * returning no results, setting the exception or, for an override value
	 * that could not be copied, the status of the override values, or every
	 * answer must be the one given with no allocation failing. Nothing may
	 * crash, and nothing may be left allocated. Once a failure is reported
	 * the results are freed, as a component whose walk failed reads as
	 * having no profile.
	 */
	void expectEveryAllocationFailureReported(
		fiftyoneDegreesPropertiesRequired properties) {
		// Made before the manager and the evidence, so that it outlasts them.
		Tracking tracking;
		// In memory, so that a read allocates nothing a cache keeps.
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			properties,
			fiftyoneDegreesHashInMemoryConfig));
		Evidence evidence(EvidencePairs({
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "User-Agent", chromeUserAgent } },
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "Sec-CH-UA",
					"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" } },
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "Sec-CH-UA-Platform", "\"Windows\"" } },
			{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
				{ std::string("51D_") + overriddenName, overriddenValue } } }));
		std::vector<std::string> expected;
		int calls = 0;
		for (int failsOn = 0; failsOn == 0 || failsOn <= calls; failsOn++) {
			const size_t allocated = fiftyoneDegreesMemoryTrackingGetAllocated();
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			startCounting(failsOn);
			fiftyoneDegreesResultsHash* results =
				fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
			bool reported = results == nullptr;
			if (reported == false) {
				fiftyoneDegreesResultsHashFromEvidence(
					results,
					evidence.array,
					exception);
				reported = FIFTYONE_DEGREES_EXCEPTION_FAILED ||
					results->b.b.overrides->status !=
						FIFTYONE_DEGREES_STATUS_SUCCESS;
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				for (int i = 0;
					reported == false && i < positionCount(results);
					i++) {
					const std::string read = readAt(results, i, exception);
					const bool failed = FIFTYONE_DEGREES_EXCEPTION_FAILED;
					FIFTYONE_DEGREES_EXCEPTION_CLEAR;
					if (failsOn == 0) {
						expected.push_back(read);
					}
					else if (read != expected[(size_t)i]) {
						EXPECT_TRUE(failed) << "With allocation " << failsOn <<
							" failing, position " << i << " read '" << read <<
							"' rather than '" << expected[(size_t)i] <<
							"', and no failure was reported.";
						reported = true;
					}
				}
				fiftyoneDegreesResultsHashFree(results);
			}
			stopCounting();
			if (failsOn == 0) {
				calls = mallocCalls;
				ASSERT_FALSE(reported) << "With no allocation failing, the "
					"results failed.";
				ASSERT_GT(calls, 0);
			}
			EXPECT_EQ(allocated, fiftyoneDegreesMemoryTrackingGetAllocated()) <<
				"Allocation " << failsOn << " of " << calls <<
				" left memory allocated.";
		}
	}

	/**
	 * Reloads the data set again and again while other threads create their
	 * own results, populate them, read every position and free them. No read
	 * may fail and nothing may crash.
	 */
	void expectReloadsWhileReadingSafe(
		fiftyoneDegreesPropertiesRequired properties) {
		ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(m, good, properties));
		std::atomic<bool> stop(false);
		std::atomic<int> failures(0);
		std::atomic<int> detections(0);
		std::vector<std::thread> threads;
		for (int t = 0; t < readerThreads; t++) {
			threads.emplace_back([&m, &stop, &failures, &detections, t]() {
				while (stop == false) {
					FIFTYONE_DEGREES_EXCEPTION_CREATE;
					fiftyoneDegreesResultsHash* results =
						fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0);
					if (results == nullptr) {
						failures++;
						continue;
					}
					const char* value = t % 2 == 0 ?
						iPhoneUserAgent : chromeUserAgent;
					fiftyoneDegreesResultsHashFromUserAgent(
						results,
						value,
						strlen(value),
						exception);
					if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
						failures++;
					}
					for (int i = 0; i < positionCount(results); i++) {
						FIFTYONE_DEGREES_EXCEPTION_CLEAR;
						fiftyoneDegreesResultsHashGetHasValues(
							results,
							i,
							exception);
						if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
							failures++;
						}
						fiftyoneDegreesResultsHashGetValues(
							results,
							i,
							exception);
						if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
							failures++;
						}
					}
					fiftyoneDegreesResultsHashFree(results);
					detections++;
				}
			});
		}
		for (int r = 0; r < reloadsWhileReading; r++) {
			reload(m, r % 2 == 0 ? renamed : good);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		stop = true;
		for (std::thread& thread : threads) {
			thread.join();
		}
		EXPECT_EQ(0, failures.load());
		EXPECT_GT(detections.load(), 0);
	}
};

// ---------------------------------------------------------------------------
// The rule: no properties named walks no graph at process, properties named
// walk exactly their components' graphs at process, and a read walks only a
// component not yet walked.
// ---------------------------------------------------------------------------

/**
 * With no properties named, populating the results walks no graph, for each
 * kind of evidence.
 */
TEST_F(HashFirstReadRuleTests, NoneNamed_ProcessWalksNoGraph) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, fiftyoneDegreesPropertiesDefault));
	const std::vector<Evidence> kinds = [] {
		std::vector<Evidence> k;
		k.push_back(userAgent(iPhoneUserAgent));
		k.push_back(Evidence(EvidencePairs({
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "Sec-CH-UA",
					"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" } },
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "Sec-CH-UA-Platform", "\"Windows\"" } } })));
		k.push_back(Evidence(EvidencePairs({
			{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
				{ "User-Agent", iPhoneUserAgent } },
			{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
				{ std::string("51D_") + overriddenName, overriddenValue } } })));
		return k;
	}();
	for (const Evidence& evidence : kinds) {
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
		std::vector<int> walks;
		countWalks(results, walks);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromEvidence(
			results,
			evidence.array,
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		EXPECT_GT(results->count, 0u);
		EXPECT_TRUE(walks.empty()) << "Populating the results walked a graph.";
		EXPECT_TRUE(noResultWalked(results)) <<
			"Populating the results walked a graph.";
		fiftyoneDegreesResultsHashFree(results);
	}
}

/**
 * With properties named in the C properties string, populating the results
 * walks the graph of each named property's component and no other. Naming
 * only a hardware property walks the hardware component's graph alone.
 */
TEST_F(HashFirstReadRuleTests, Named_String_ProcessWalksOnlyTheNamedComponents) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	for (const std::string& names : {
		std::string(hardwareProperty),
		std::string(hardwareProperty) + "," + browserProperty }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(m, named(names.c_str())));
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
		std::vector<int> expected;
		for (const char* name : { hardwareProperty, browserProperty }) {
			if (names.find(name) != std::string::npos) {
				expected.push_back(componentOfName(results, name));
			}
		}
		std::sort(expected.begin(), expected.end());
		EXPECT_EQ(expected, availableComponents(results)) << names;
		std::vector<int> walks;
		countWalks(results, walks);
		populate(results, userAgent(iPhoneUserAgent));
		EXPECT_EQ(expected.size(), (size_t)results->count) << names;
		EXPECT_TRUE(everyResultWalked(results)) <<
			"Populating the results did not walk the graph of each named "
			"property's component, " << names;
		if (walkMethod != nullptr) {
			EXPECT_EQ(expected, walks) << names;
		}
		fiftyoneDegreesResultsHashFree(results);
	}
}

/**
 * The same with the properties named in the C properties array.
 */
TEST_F(HashFirstReadRuleTests, Named_Array_ProcessWalksOnlyTheNamedComponents) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const char* names[] = { hardwareProperty };
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.array = names;
	properties.count = 1;
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, properties));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
	const std::vector<int> expected = {
		componentOfName(results, hardwareProperty) };
	EXPECT_EQ(expected, availableComponents(results));
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, userAgent(iPhoneUserAgent));
	EXPECT_EQ(1u, results->count);
	EXPECT_TRUE(everyResultWalked(results)) <<
		"Populating the results did not walk the hardware component's graph.";
	if (walkMethod != nullptr) {
		EXPECT_EQ(expected, walks);
	}
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * The same with the properties named through the C++ builder, the required
 * properties configuration the engine is created with. With none named, the
 * engine's process walks no graph.
 */
TEST_F(HashFirstReadRuleTests, Builder_ProcessWalksOnlyTheNamedComponents) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ConfigHash config;
	config.setBalanced();
	{
		const std::vector<std::string> names = { hardwareProperty };
		RequiredPropertiesConfig properties(&names);
		EngineHash engine(dataFilePath.c_str(), &config, &properties);
		std::unique_ptr<ResultsHash> results(engine.process(iPhoneUserAgent));
		fiftyoneDegreesResultsHash* c = cResultsOf(results.get());
		EXPECT_EQ(
			std::vector<int>({ componentOfName(c, hardwareProperty) }),
			availableComponents(c));
		EXPECT_EQ(1u, c->count);
		EXPECT_TRUE(everyResultWalked(c)) <<
			"The engine's process did not walk the hardware component's "
			"graph.";
	}
	{
		RequiredPropertiesConfig properties;
		EngineHash engine(dataFilePath.c_str(), &config, &properties);
		std::unique_ptr<ResultsHash> results(engine.process(iPhoneUserAgent));
		fiftyoneDegreesResultsHash* c = cResultsOf(results.get());
		EXPECT_GT(c->count, 0u);
		EXPECT_TRUE(noResultWalked(c)) <<
			"With no properties named, the engine's process walked a graph.";
	}
}

/**
 * After populating the results of an engine with properties named, no read
 * walks a graph, by name or by position.
 */
TEST_F(HashFirstReadRuleTests, Named_ReadsWalkNothing) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::string names = onePropertyOfEachComponent();
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, named(names.c_str())));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, userAgent(iPhoneUserAgent));
	EXPECT_TRUE(everyResultWalked(results));
	const size_t walked = walks.size();
	std::vector<int32_t> iterations;
	for (uint32_t i = 0; i < results->count; i++) {
		iterations.push_back(results->items[i].iterations);
	}
	describeAll(results);
	fiftyoneDegreesPropertiesAvailable* available =
		dataSetOf(results)->b.b.available;
	for (uint32_t i = 0; i < available->count; i++) {
		valueOf(results, FIFTYONE_DEGREES_STRING(
			available->items[i].name.data.ptr));
	}
	EXPECT_EQ(walked, walks.size()) << "A read walked a graph.";
	for (uint32_t i = 0; i < results->count; i++) {
		EXPECT_EQ(iterations[i], results->items[i].iterations) <<
			"A read walked the graph of result " << i << " again.";
	}
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * With no properties named, a read walks the graph of its property's
 * component alone, once, and a read of another component's property walks
 * that component.
 */
TEST_F(HashFirstReadRuleTests, NoneNamed_ReadWalksOnlyItsComponent) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, fiftyoneDegreesPropertiesDefault));
	OwnedResults results(
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1),
		fiftyoneDegreesResultsHashFree);
	const int hardware = componentOfName(results.get(), hardwareProperty);
	const int browser = componentOfName(results.get(), browserProperty);
	ASSERT_GE(hardware, 0);
	ASSERT_GE(browser, 0);
	const std::vector<int> components = availableComponents(results.get());
	std::vector<int> walks;
	countWalks(results.get(), walks);
	populate(results.get(), userAgent(iPhoneUserAgent));
	ASSERT_EQ(components.size(), (size_t)results->count);
	valueOf(results.get(), hardwareProperty);
	valueOf(results.get(), hardwareProperty);
	EXPECT_EQ(std::vector<int>({ hardware }), walks);
	std::vector<bool> expected(components.size(), false);
	for (size_t i = 0; i < components.size(); i++) {
		expected[i] = components[i] == hardware;
	}
	EXPECT_EQ(expected, walkedResults(results.get())) <<
		"Reading a hardware property walked another component's graph.";
	valueOf(results.get(), browserProperty);
	EXPECT_EQ(std::vector<int>({ hardware, browser }), walks);
}

/**
 * Populating the results of an engine with properties named allocates
 * nothing when the data set is in memory, as the results walk every graph
 * they need while they are populated and so keep no copy of the evidence.
 */
TEST_F(HashFirstReadRuleTests, Named_PopulatingAllocatesNothing) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::string names = onePropertyOfEachComponent();
	// Made before the manager and the evidence, so that it outlasts them.
	Tracking tracking;
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(
		m,
		named(names.c_str()),
		fiftyoneDegreesHashInMemoryConfig));
	Evidence evidence(EvidencePairs({
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "User-Agent", chromeUserAgent } },
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "Sec-CH-UA",
				"\"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\"" } },
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "Sec-CH-UA-Mobile", "?0" } },
		{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			{ "Sec-CH-UA-Platform", "\"Windows\"" } } }));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	startCounting(0);
	fiftyoneDegreesResultsHashFromEvidence(results, evidence.array, exception);
	stopCounting();
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	EXPECT_GT(results->count, 0u);
	EXPECT_EQ(0, mallocCalls) << "Populating the results allocated.";
	fiftyoneDegreesResultsHashFree(results);
}

// ---------------------------------------------------------------------------
// Unhappy paths.
// ---------------------------------------------------------------------------

/**
 * With properties named, a graph that cannot be read fails populating the
 * results, which reports it as before first reads.
 */
TEST_F(HashFirstReadRuleTests, Named_GraphCannotBeRead_ProcessReportsIt) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::string names = onePropertyOfEachComponent();
	Manager m;
	ASSERT_NO_FATAL_FAILURE(initWithGraphsCut(m, named(names.c_str())));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0);
	Evidence evidence = userAgent(iPhoneUserAgent);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromEvidence(results, evidence.array, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED) <<
		"Populating the results did not report the failed graph read.";
	fiftyoneDegreesResultsHashFree(results);
	m.release();
	removeCopy();
}

/**
 * With no properties named, populating the results reads no graph, the first
 * read of a property reports the failed graph read, and the component is not
 * walked again for the results, so later reads find no profile.
 */
TEST_F(HashFirstReadRuleTests, NoneNamed_GraphCannotBeRead_FirstReadReportsIt) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	Manager m;
	ASSERT_NO_FATAL_FAILURE(
		initWithGraphsCut(m, fiftyoneDegreesPropertiesDefault));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0);
	Evidence evidence = userAgent(iPhoneUserAgent);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromEvidence(results, evidence.array, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"Populating the results read a graph.";
	const int position = 0;
	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	fiftyoneDegreesResultsHashGetValues(results, position, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED) <<
		"The first read did not report the failed graph read.";
	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	EXPECT_FALSE(fiftyoneDegreesResultsHashGetHasValues(
		results,
		position,
		exception)) << "A later read did not read as if there were no profile.";
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"A later read walked the graph again.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE,
		fiftyoneDegreesResultsHashGetNoValueReason(
			results,
			position,
			exception));
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesResultsHashFree(results);
	m.release();
	removeCopy();
}

/**
 * With properties named, every allocation of creating, populating and
 * reading the results failing in turn is reported and leaves nothing.
 */
TEST_F(HashFirstReadRuleTests, Named_EveryAllocationFails_IsReported) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	const std::string names = onePropertyOfEachComponent() + "," +
		overriddenName;
	expectEveryAllocationFailureReported(named(names.c_str()));
}

/**
 * The same with no properties named, where populating the results copies
 * the evidence and the first reads walk the graphs.
 */
TEST_F(HashFirstReadRuleTests, NoneNamed_EveryAllocationFails_IsReported) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	expectEveryAllocationFailureReported(fiftyoneDegreesPropertiesDefault);
}

/**
 * Evidence that answers no component, no evidence, and evidence that only
 * overrides property values, profiles or the device walk no graph, in either
 * case, with and without unmatched results.
 */
TEST_F(HashFirstReadRuleTests, EvidenceForNoComponent_WalksNoGraph) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	std::string deviceId, profileId;
	{
		Manager all;
		ASSERT_NO_FATAL_FAILURE(init(all, fiftyoneDegreesPropertiesDefault));
		fiftyoneDegreesResultsHash* first =
			fiftyoneDegreesResultsHashCreate(&all.manager, 0, 0);
		populate(first, userAgent(iPhoneUserAgent));
		char id[256] = "";
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesHashGetDeviceIdFromResults(
			first,
			id,
			sizeof(id),
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		deviceId = id;
		profileId = deviceId.substr(0, deviceId.find('-'));
		fiftyoneDegreesResultsHashFree(first);
	}
	const std::string names = onePropertyOfEachComponent() + "," +
		overriddenName;
	for (const bool allowUnmatched : { false, true }) {
		for (const bool withNames : { false, true }) {
			fiftyoneDegreesConfigHash config =
				fiftyoneDegreesHashBalancedConfig;
			config.b.allowUnmatched = allowUnmatched;
			Manager m;
			ASSERT_NO_FATAL_FAILURE(init(
				m,
				withNames ?
					named(names.c_str()) : fiftyoneDegreesPropertiesDefault,
				config));
			std::vector<Evidence> kinds;
			kinds.push_back(Evidence(EvidencePairs()));
			kinds.push_back(Evidence(EvidencePairs({
				{ FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
					{ "Accept", "text/html" } } })));
			kinds.push_back(Evidence(EvidencePairs({
				{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
					{ std::string("51D_") + overriddenName,
						overriddenValue } } })));
			kinds.push_back(Evidence(EvidencePairs({
				{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
					{ "51D_ProfileIds", profileId } } })));
			kinds.push_back(Evidence(EvidencePairs({
				{ FIFTYONE_DEGREES_EVIDENCE_QUERY,
					{ "51D_deviceId", deviceId } } })));
			for (const Evidence& evidence : kinds) {
				fiftyoneDegreesResultsHash* results =
					fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
				std::vector<int> walks;
				countWalks(results, walks);
				FIFTYONE_DEGREES_EXCEPTION_CREATE;
				fiftyoneDegreesResultsHashFromEvidence(
					results,
					evidence.array,
					exception);
				FIFTYONE_DEGREES_EXCEPTION_THROW;
				EXPECT_TRUE(walks.empty()) << "A graph was walked, " <<
					(withNames ? "named" : "none named");
				EXPECT_TRUE(noResultWalked(results)) <<
					"A graph was walked, " <<
					(withNames ? "named" : "none named");
				describeAll(results);
				EXPECT_TRUE(walks.empty()) << "A read walked a graph, " <<
					(withNames ? "named" : "none named");
				fiftyoneDegreesResultsHashFree(results);
			}
		}
	}
}

/**
 * A property the data set does not have reads as missing, by name and by
 * position, with properties named and with none, and walks no graph.
 */
TEST_F(HashFirstReadRuleTests, UnavailableProperty_ReadsAsMissing) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	for (const bool withNames : { false, true }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			withNames ?
				named(hardwareProperty) : fiftyoneDegreesPropertiesDefault));
		fiftyoneDegreesResultsHash* results =
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 1);
		populate(results, userAgent(iPhoneUserAgent));
		std::vector<int> walks;
		countWalks(results, walks);
		const char* missing = withNames ? browserProperty : "NoSuchProperty";
		EXPECT_EQ(std::string(), valueOf(results, missing)) << missing;
		const int beyond = positionCount(results);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_FALSE(fiftyoneDegreesResultsHashGetHasValues(
			results,
			beyond,
			exception));
		EXPECT_EQ(
			FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
			fiftyoneDegreesResultsHashGetNoValueReason(
				results,
				beyond,
				exception));
		EXPECT_EQ(nullptr, fiftyoneDegreesResultsHashGetValues(
			results,
			beyond,
			exception));
		FIFTYONE_DEGREES_EXCEPTION_CLEAR;
		EXPECT_TRUE(walks.empty()) <<
			"Reading a property the data set does not have walked a graph, " <<
			(withNames ? "named" : "none named");
		fiftyoneDegreesResultsHashFree(results);
	}
}

// ---------------------------------------------------------------------------
// Reloads.
// ---------------------------------------------------------------------------

/**
 * Results populated before a reload and read after it read the data set
 * they hold, with properties named and with none, so they answer as they
 * would have before the reload, which the reloaded data set does not.
 */
TEST_F(HashFirstReadRuleTests, Reload_ReadAfterReload_AnswersFromTheHeldDataSet) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	for (const bool withNames : { false, true }) {
		const std::string names = renamedName + "," + hardwareProperty;
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			m,
			good,
			withNames ?
				named(names.c_str()) : fiftyoneDegreesPropertiesDefault));
		OwnedResults expected(
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		OwnedResults held(
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		populate(expected.get(), userAgent(iPhoneUserAgent));
		populate(held.get(), userAgent(iPhoneUserAgent));
		const std::vector<std::string> before = describeAll(expected.get());
		expected.reset();

		ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
		EXPECT_EQ(before, describeAll(held.get())) <<
			(withNames ? "named" : "none named");

		OwnedResults reloaded(
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		populate(reloaded.get(), userAgent(iPhoneUserAgent));
		EXPECT_NE(before, describeAll(reloaded.get())) <<
			"The reload did not change what the positions read, " <<
			(withNames ? "named" : "none named");
	}
}

/**
 * Results created before a reload and populated again after it keep the data
 * set they were created with, as they hold it until they are freed, with
 * properties named and with none, while results created after the reload
 * use the reloaded data set.
 */
TEST_F(HashFirstReadRuleTests, Reload_ReusedResults_KeepTheirDataSet) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	for (const bool withNames : { false, true }) {
		const std::string names = renamedName + "," + hardwareProperty;
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			m,
			good,
			withNames ?
				named(names.c_str()) : fiftyoneDegreesPropertiesDefault));
		OwnedResults reused(
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		populate(reused.get(), userAgent(iPhoneUserAgent));
		const std::vector<std::string> before = describeAll(reused.get());
		const void* held = reused->b.b.dataSet;

		ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
		populate(reused.get(), userAgent(iPhoneUserAgent));
		EXPECT_EQ(held, reused->b.b.dataSet);
		EXPECT_EQ(before, describeAll(reused.get())) <<
			(withNames ? "named" : "none named");

		OwnedResults fresh(
			fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0),
			fiftyoneDegreesResultsHashFree);
		EXPECT_NE(held, fresh->b.b.dataSet);
		populate(fresh.get(), userAgent(iPhoneUserAgent));
		EXPECT_NE(before, describeAll(fresh.get())) <<
			(withNames ? "named" : "none named");
	}
}

/**
 * A reload to a file that lacks a named property keeps the property's
 * position, which reads as a property the data set does not have, and
 * populating the results walks the graphs of the named properties the file
 * has, without failing.
 */
TEST_F(HashFirstReadRuleTests, Reload_NamedPropertyMissing_ProcessSkipsIt) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	const std::string names = renamedName + "," + hardwareProperty;
	Manager m;
	ASSERT_NO_FATAL_FAILURE(initFromMemory(m, good, named(names.c_str())));
	ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
	fiftyoneDegreesResultsHash* results =
		fiftyoneDegreesResultsHashCreate(&m.manager, 0, 0);
	std::vector<int> walks;
	countWalks(results, walks);
	Evidence evidence = userAgent(iPhoneUserAgent);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHashFromEvidence(results, evidence.array, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"Populating the results failed for a named property the file lacks.";
	EXPECT_TRUE(everyResultWalked(results));
	EXPECT_EQ(availableComponents(results).size(), (size_t)results->count);
	EXPECT_EQ(2, positionCount(results));
	EXPECT_FALSE(fiftyoneDegreesResultsHashGetHasValues(
		results,
		0,
		exception));
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
		fiftyoneDegreesResultsHashGetNoValueReason(results, 0, exception));
	EXPECT_TRUE(fiftyoneDegreesResultsHashGetHasValues(
		results,
		1,
		exception)) << "The named property the file has gave no value.";
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	fiftyoneDegreesResultsHashFree(results);
}

/**
 * Reloads made while other threads populate and read their own results
 * neither fail a read nor crash, with no properties named.
 */
TEST_F(HashFirstReadRuleTests, NoneNamed_ReloadWhileReading_IsSafe) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	expectReloadsWhileReadingSafe(fiftyoneDegreesPropertiesDefault);
}

/**
 * The same with properties named.
 */
TEST_F(HashFirstReadRuleTests, Named_ReloadWhileReading_IsSafe) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	expectReloadsWhileReadingSafe(named(hardwareProperty));
}
