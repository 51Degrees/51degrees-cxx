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
 * @file IpiFirstReadRuleTests.cpp
 * @brief Tests when IP Intelligence results walk a component's graph. With
 * no properties named when the data set was created, populating the results
 * walks no graph. With properties named, populating walks the graph of each
 * named property's component and no other. Either way, a read walks its
 * property's component only if it has not been walked. The tests name
 * properties with the C properties string and array and with the C++
 * builder, and cover a graph that cannot be read, every allocation failing
 * in turn, evidence that answers no component, properties that are not
 * available, and reloads.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

namespace {

/** The data file committed to the data submodule, so every build has it. */
const char smallFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** Addresses the data files have answers for. */
const char* const addresses[] = { "1.1.1.1", "8.8.8.8" };

/** Addresses of both versions and forms, and the ends of the ranges. */
const char* const everyAddress[] = {
	"50.154.29.201",
	"1.1.1.1",
	"8.8.8.8",
	"185.28.167.77",
	"2001:4860:4860::8888",
	"::ffff:50.154.29.201",
	"0.0.0.0",
	"255.255.255.255"
};

/** Bytes for the text of a property's values. */
const size_t valueBufferBytes = 1 << 16;

/** Threads that use their own results while the data set is reloaded. */
const int readerThreads = 4;

/** Reloads made while the threads use their own results. */
const int reloadsWhileReading = 12;

/** Pause between the reloads made while the threads read. */
const int reloadPauseMilliseconds = 5;

/** The components walked, in order, by the results that count their walks. */
std::vector<int>* walkLog = nullptr;

/** The walk the results make when they are not counting, or null. */
fiftyoneDegreesResultsEvaluateMethod walkMethod = nullptr;

/** Records the component in the log, then walks it. */
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
void countWalks(fiftyoneDegreesResultsIpi* results, std::vector<int>& log) {
	walkMethod = results->b.evaluate;
	walkLog = &log;
	results->b.evaluate = countWalk;
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

fiftyoneDegreesDataSetIpi* dataSetOf(fiftyoneDegreesResultsIpi* results) {
	return (fiftyoneDegreesDataSetIpi*)results->b.dataSet;
}

/** True if a component of the results is still to be walked. */
bool anyPending(fiftyoneDegreesResultsIpi* results) {
	for (const uint32_t word : results->b.pending) {
		if (word != 0) {
			return true;
		}
	}
	return false;
}

/** The available components of the results' data set, in order. */
std::vector<int> availableComponents(fiftyoneDegreesResultsIpi* results) {
	fiftyoneDegreesDataSetIpi* dataSet = dataSetOf(results);
	std::vector<int> components;
	for (uint32_t i = 0; i < dataSet->componentsList.count; i++) {
		if (dataSet->componentsAvailable[i]) {
			components.push_back((int)i);
		}
	}
	return components;
}

/** The components of the results, in the order the results hold them. */
std::vector<int> componentsOf(fiftyoneDegreesResultsIpi* results) {
	std::vector<int> components;
	for (uint32_t i = 0; i < results->count; i++) {
		components.push_back((int)results->items[i].componentIndex);
	}
	return components;
}

/** Every position of the names given, or of the properties when none. */
int positionCount(fiftyoneDegreesResultsIpi* results) {
	return (int)fiftyoneDegreesPropertiesGetRequiredPropertyCount(
		dataSetOf(results)->b.b.available);
}

/** The values of the property at the position, or why it has none. */
std::string describe(fiftyoneDegreesResultsIpi* results, int position) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	if (fiftyoneDegreesResultsIpiGetHasValues(
		results,
		position,
		exception) == false) {
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		const int reason = (int)fiftyoneDegreesResultsIpiGetNoValueReason(
			results,
			position,
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		return "no value, reason " + std::to_string(reason);
	}
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	std::vector<char> buffer(valueBufferBytes, '\0');
	fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
		results,
		position,
		buffer.data(),
		buffer.size(),
		"|",
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return std::string(buffer.data());
}

/** What every position reads, in order. */
std::vector<std::string> describeAll(fiftyoneDegreesResultsIpi* results) {
	std::vector<std::string> values;
	for (int i = 0; i < positionCount(results); i++) {
		values.push_back(describe(results, i));
	}
	return values;
}

/** The status and the text of the values of the property named. */
std::string readByName(fiftyoneDegreesResultsIpi* results, const char* name) {
	std::vector<char> buffer(valueBufferBytes, '\0');
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiGetValuesString(
		results,
		name,
		buffer.data(),
		buffer.size(),
		"|",
		exception);
	return std::to_string((int)exception->status) + " " +
		std::string(buffer.data());
}

/** The status and the text a read of the position gives. */
std::string readAt(
	fiftyoneDegreesResultsIpi* results,
	int position,
	fiftyoneDegreesException* exception) {
	std::vector<char> buffer(valueBufferBytes, '\0');
	fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
		results,
		position,
		buffer.data(),
		buffer.size(),
		"|",
		exception);
	return std::to_string((int)exception->status) + " " +
		std::string(buffer.data());
}

/** Populates the results for the address, failing the test on an error. */
void populate(fiftyoneDegreesResultsIpi* results, const char* address) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		address,
		strlen(address),
		exception);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
}

/** The names given, separated by commas. */
std::string join(const std::vector<std::string>& names) {
	std::string list;
	for (const std::string& name : names) {
		list += (list.empty() ? "" : ",") + name;
	}
	return list;
}

/** A property's name and the index of its component. */
typedef std::pair<std::string, int> NamedComponent;

} // namespace

/**
 * Tests through the C API on the first IP Intelligence data file found, and
 * through the C++ classes, whose results the tests read as a friend of the
 * class. The tests that copy, cut or reload a data file use the small data
 * file in the data submodule.
 */
class IpiFirstReadRuleTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0;
			i < _IpiFileNamesLength && dataFilePath.empty();
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No IP Intelligence data file was found.";
		}
		smallFilePath = GetFilePath(_dataFolderName, smallFileName);
	}

	void TearDown() override {
		walkLog = nullptr;
		walkMethod = nullptr;
		removeCopy();
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::string().swap(dataFilePath);
		std::string().swap(smallFilePath);
		std::string().swap(copyPath);
		std::vector<unsigned char>().swap(good);
		std::vector<unsigned char>().swap(renamed);
		std::string().swap(renamedName);
		std::string().swap(keptName);
		Base::TearDown();
	}

protected:
	std::string dataFilePath;
	std::string smallFilePath;
	std::string copyPath;
	std::vector<unsigned char> good;
	std::vector<unsigned char> renamed;
	std::string renamedName;
	std::string keptName;

	/** A manager for a data file, freed when it goes. */
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
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiBalancedConfig,
		const std::string& path = std::string()) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
				&m.manager,
				&config,
				&properties,
				path.empty() ? dataFilePath.c_str() : path.c_str(),
				exception));
		m.ready = true;
	}

	/** Properties naming those in the string. */
	static fiftyoneDegreesPropertiesRequired named(const char* names) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = names;
		return properties;
	}

	/** The C results a C++ results instance holds. */
	static fiftyoneDegreesResultsIpi* cResultsOf(ResultsIpi* results) {
		return results->results;
	}

	/** Every property of the data file, in name order, with its component. */
	std::vector<NamedComponent> propertiesOf(const std::string& path) {
		std::vector<NamedComponent> properties;
		Manager all;
		init(all, fiftyoneDegreesPropertiesDefault,
			fiftyoneDegreesIpiBalancedConfig, path);
		if (all.ready == false) {
			return properties;
		}
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&all.manager);
		fiftyoneDegreesPropertiesAvailable* available =
			dataSet->b.b.available;
		for (uint32_t i = 0; i < available->count; i++) {
			properties.push_back(NamedComponent(
				FIFTYONE_DEGREES_STRING(available->items[i].name.data.ptr),
				(int)available->items[i].componentIndex));
		}
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		return properties;
	}

	/**
	 * The first property in name order, and the first property on another
	 * component, or the first alone if every property is on one component.
	 */
	static std::vector<std::string> twoNames(
		const std::vector<NamedComponent>& properties) {
		std::vector<std::string> names;
		for (const NamedComponent& property : properties) {
			if (names.empty() ||
				(names.size() == 1 &&
					property.second != properties[0].second)) {
				names.push_back(property.first);
			}
		}
		return names;
	}

	/** The first property in name order of each component. */
	static std::vector<std::string> onePerComponent(
		const std::vector<NamedComponent>& properties) {
		std::vector<int> components;
		std::vector<std::string> names;
		for (const NamedComponent& property : properties) {
			if (std::find(
				components.begin(),
				components.end(),
				property.second) == components.end()) {
				components.push_back(property.second);
				names.push_back(property.first);
			}
		}
		return names;
	}

	/** The name of every property. */
	static std::vector<std::string> allNames(
		const std::vector<NamedComponent>& properties) {
		std::vector<std::string> names;
		for (const NamedComponent& property : properties) {
			names.push_back(property.first);
		}
		return names;
	}

	/**
	 * Copies the small data file with the first property in name order
	 * renamed so that it sorts after every other name, keeping the name of
	 * the second, which the copy still has. The name is changed in place, so
	 * the layout of the file is the same, and a data set loaded from the copy
	 * lacks the property under its old name.
	 */
	void renameFirstProperty() {
		ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
			"' was not found.";
		FILE* file = fopen(smallFilePath.c_str(), "rb");
		ASSERT_NE(nullptr, file);
		fseek(file, 0, SEEK_END);
		good.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(good.data(), 1, good.size(), file);
		fclose(file);
		ASSERT_EQ(good.size(), bytes);
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				good.data(),
				(fiftyoneDegreesFileOffset)good.size(),
				exception));
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		fiftyoneDegreesPropertiesAvailable* available =
			dataSet->b.b.available;
		const bool two = available->count >= 2;
		const fiftyoneDegreesPropertyAvailable* first = &available->items[0];
		renamedName = FIFTYONE_DEGREES_STRING(first->name.data.ptr);
		if (two) {
			keptName = FIFTYONE_DEGREES_STRING(
				available->items[1].name.data.ptr);
		}
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
				property->nameOffset + sizeof(uint16_t) :
			0;
		if (found) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		}
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);
		ASSERT_TRUE(two) << "The data file has fewer than two properties.";
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
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromMemory(
				&m.manager,
				&config,
				&properties,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception));
		m.ready = true;
	}

	/** Reloads the manager from the memory given. */
	static void reload(Manager& m, std::vector<unsigned char>& data) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiReloadManagerFromMemory(
				&m.manager,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception));
	}

	/** The first byte of the data file that any graph is read from. */
	static uint64_t firstGraphByte(const fiftyoneDegreesDataSetIpi* dataSet) {
		uint64_t first = UINT64_MAX;
		for (uint32_t i = 0; i < dataSet->graphsArray->count; i++) {
			const fiftyoneDegreesIpiCgInfo* info =
				&dataSet->graphsArray->items[i].info;
			const uint64_t starts[] = {
				(uint64_t)info->spanBytes.startPosition,
				(uint64_t)info->spans.startPosition,
				(uint64_t)info->clusters.startPosition,
				(uint64_t)info->nodes.collection.startPosition };
			for (const uint64_t start : starts) {
				if (start < first) {
					first = start;
				}
			}
		}
		return first;
	}

	/**
	 * Copies the small data file and returns a manager for the copy whose
	 * graphs are read from the file, with the properties, values and strings
	 * in memory, then cuts the copy short where the first graph begins, so
	 * that every graph read fails while everything else can still be read.
	 */
	void initWithGraphsCut(
		Manager& m,
		fiftyoneDegreesPropertiesRequired properties) {
		ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
			"' was not found.";
		copyPath = std::string("ipi-first-read-rule-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".ipi";
		std::filesystem::copy_file(
			smallFilePath,
			copyPath,
			std::filesystem::copy_options::overwrite_existing);
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
		config.properties.loaded = true;
		config.values.loaded = true;
		config.strings.loaded = true;
		ASSERT_NO_FATAL_FAILURE(init(m, properties, config, copyPath));
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&m.manager);
		const uint64_t cut = firstGraphByte(dataSet);
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		ASSERT_NE(UINT64_MAX, cut) << "The data file has no graph.";
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
	 * returning no results or setting the exception, or every answer must be
	 * the one given with no allocation failing, as when the memory was for a
	 * cache the results can do without. Nothing may crash, and nothing may be
	 * left allocated. Once a failure is reported the results are freed, as a
	 * component whose walk failed reads as having no profile.
	 */
	void expectEveryAllocationFailureReported(
		fiftyoneDegreesPropertiesRequired properties) {
		ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
			"' was not found.";
		// Made before the manager, so that it outlasts it.
		Tracking tracking;
		// In memory, so that a read allocates nothing a cache keeps.
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			properties,
			fiftyoneDegreesIpiInMemoryConfig,
			smallFilePath));
		std::vector<std::string> expected;
		int calls = 0;
		for (int failsOn = 0; failsOn == 0 || failsOn <= calls; failsOn++) {
			const size_t allocated = fiftyoneDegreesMemoryTrackingGetAllocated();
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			startCounting(failsOn);
			fiftyoneDegreesResultsIpi* results =
				fiftyoneDegreesResultsIpiCreate(&m.manager);
			bool reported = results == nullptr;
			if (reported == false) {
				fiftyoneDegreesResultsIpiFromIpAddressString(
					results,
					addresses[0],
					strlen(addresses[0]),
					exception);
				reported = FIFTYONE_DEGREES_EXCEPTION_FAILED;
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
				fiftyoneDegreesResultsIpiFree(results);
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
	 * may fail and nothing may crash. With names, the property named is one
	 * both files have.
	 */
	void expectReloadsWhileReadingSafe(bool withNames) {
		ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			m,
			good,
			withNames ?
				named(keptName.c_str()) : fiftyoneDegreesPropertiesDefault));
		std::atomic<bool> stop(false);
		std::atomic<int> failures(0);
		std::atomic<int> detections(0);
		std::vector<std::thread> threads;
		for (int t = 0; t < readerThreads; t++) {
			threads.emplace_back([&m, &stop, &failures, &detections, t]() {
				while (stop == false) {
					FIFTYONE_DEGREES_EXCEPTION_CREATE;
					fiftyoneDegreesResultsIpi* results =
						fiftyoneDegreesResultsIpiCreate(&m.manager);
					if (results == nullptr) {
						failures++;
						continue;
					}
					const char* address = addresses[t % 2];
					fiftyoneDegreesResultsIpiFromIpAddressString(
						results,
						address,
						strlen(address),
						exception);
					if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
						failures++;
					}
					for (int i = 0; i < positionCount(results); i++) {
						FIFTYONE_DEGREES_EXCEPTION_CLEAR;
						fiftyoneDegreesResultsIpiGetHasValues(
							results,
							i,
							exception);
						if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
							failures++;
						}
						fiftyoneDegreesResultsIpiGetValues(
							results,
							i,
							exception);
						if (FIFTYONE_DEGREES_EXCEPTION_FAILED) {
							failures++;
						}
					}
					fiftyoneDegreesResultsIpiFree(results);
					detections++;
				}
			});
		}
		for (int r = 0; r < reloadsWhileReading; r++) {
			reload(m, r % 2 == 0 ? renamed : good);
			std::this_thread::sleep_for(
				std::chrono::milliseconds(reloadPauseMilliseconds));
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
 * With no properties named, populating the results walks no graph.
 */
TEST_F(IpiFirstReadRuleTests, NoneNamed_ProcessWalksNoGraph) {
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, fiftyoneDegreesPropertiesDefault));
	for (const char* address : addresses) {
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		std::vector<int> walks;
		countWalks(results, walks);
		populate(results, address);
		EXPECT_GT(results->count, 0u);
		EXPECT_TRUE(walks.empty()) << "Populating the results walked a graph.";
		EXPECT_TRUE(anyPending(results));
		fiftyoneDegreesResultsIpiFree(results);
	}
}

/**
 * With properties named in the C properties string, populating the results
 * walks the graph of each named property's component and no other.
 */
TEST_F(IpiFirstReadRuleTests, Named_String_ProcessWalksOnlyTheNamedComponents) {
	const std::vector<std::string> names =
		twoNames(propertiesOf(dataFilePath));
	ASSERT_FALSE(names.empty());
	for (size_t count = 1; count <= names.size(); count++) {
		const std::string list = join(std::vector<std::string>(
			names.begin(),
			names.begin() + (std::ptrdiff_t)count));
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(m, named(list.c_str())));
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		const std::vector<int> expected = availableComponents(results);
		EXPECT_EQ(count, expected.size()) << list;
		std::vector<int> walks;
		countWalks(results, walks);
		populate(results, addresses[0]);
		EXPECT_EQ(expected, componentsOf(results)) << list;
		EXPECT_EQ(expected, walks) <<
			"Populating the results did not walk the graph of each named "
			"property's component alone, " << list;
		EXPECT_FALSE(anyPending(results)) << list;
		fiftyoneDegreesResultsIpiFree(results);
	}
}

/**
 * The same with the properties named in the C properties array.
 */
TEST_F(IpiFirstReadRuleTests, Named_Array_ProcessWalksOnlyTheNamedComponents) {
	const std::vector<std::string> names =
		twoNames(propertiesOf(dataFilePath));
	ASSERT_FALSE(names.empty());
	const char* array[] = { names[0].c_str() };
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.array = array;
	properties.count = 1;
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, properties));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	const std::vector<int> expected = availableComponents(results);
	EXPECT_EQ(1u, expected.size());
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, addresses[0]);
	EXPECT_EQ(expected, walks) <<
		"Populating the results did not walk the named property's component "
		"alone.";
	EXPECT_FALSE(anyPending(results));
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * The same with the properties named through the C++ builder, the required
 * properties configuration the engine is created with. With none named, the
 * engine's process walks no graph.
 */
TEST_F(IpiFirstReadRuleTests, Builder_ProcessWalksOnlyTheNamedComponents) {
	const std::vector<std::string> names =
		twoNames(propertiesOf(dataFilePath));
	ASSERT_FALSE(names.empty());
	ConfigIpi config;
	config.setBalanced();
	{
		const std::vector<std::string> first = { names[0] };
		RequiredPropertiesConfig properties(&first);
		EngineIpi engine(dataFilePath.c_str(), &config, &properties);
		std::unique_ptr<ResultsIpi> results(engine.process(addresses[0]));
		fiftyoneDegreesResultsIpi* c = cResultsOf(results.get());
		EXPECT_EQ(1u, availableComponents(c).size());
		EXPECT_EQ(1u, c->count);
		EXPECT_FALSE(anyPending(c)) <<
			"The engine's process did not walk the named property's "
			"component.";
	}
	{
		RequiredPropertiesConfig properties;
		EngineIpi engine(dataFilePath.c_str(), &config, &properties);
		std::unique_ptr<ResultsIpi> results(engine.process(addresses[0]));
		fiftyoneDegreesResultsIpi* c = cResultsOf(results.get());
		EXPECT_GT(c->count, 0u);
		EXPECT_TRUE(anyPending(c)) <<
			"With no properties named, the engine's process walked a graph.";
	}
}

/**
 * After populating the results of an engine with properties named, no read
 * walks a graph, by name or by position.
 */
TEST_F(IpiFirstReadRuleTests, Named_ReadsWalkNothing) {
	const std::vector<std::string> names =
		onePerComponent(propertiesOf(dataFilePath));
	const std::string list = join(names);
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, named(list.c_str())));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, addresses[0]);
	EXPECT_EQ(availableComponents(results), walks);
	const size_t walked = walks.size();
	describeAll(results);
	for (const std::string& name : names) {
		readByName(results, name.c_str());
	}
	EXPECT_EQ(walked, walks.size()) << "A read walked a graph.";
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * With no properties named, a read walks the graph of its property's
 * component alone, once.
 */
TEST_F(IpiFirstReadRuleTests, NoneNamed_ReadWalksOnlyItsComponent) {
	Manager m;
	ASSERT_NO_FATAL_FAILURE(init(m, fiftyoneDegreesPropertiesDefault));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	std::vector<int> walks;
	countWalks(results, walks);
	populate(results, addresses[0]);
	fiftyoneDegreesPropertiesAvailable* available =
		dataSetOf(results)->b.b.available;
	const int position = 0;
	const int component = (int)available->items[
		fiftyoneDegreesPropertiesGetAvailableIndex(available, position)]
		.componentIndex;
	describe(results, position);
	describe(results, position);
	EXPECT_EQ(std::vector<int>({ component }), walks) <<
		"A read walked another component's graph, or its own twice.";
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * The results of an engine with properties named give the answers of an
 * engine with none named, for every address, whatever order the properties
 * are read in, with every property named and with one of each component.
 */
TEST_F(IpiFirstReadRuleTests, Named_AnswersAsNoneNamed) {
	const std::vector<NamedComponent> properties = propertiesOf(dataFilePath);
	Manager none;
	ASSERT_NO_FATAL_FAILURE(init(none, fiftyoneDegreesPropertiesDefault));
	for (const std::vector<std::string>& names : {
		allNames(properties),
		onePerComponent(properties) }) {
		const std::string list = join(names);
		Manager some;
		ASSERT_NO_FATAL_FAILURE(init(some, named(list.c_str())));
		for (const char* address : everyAddress) {
			fiftyoneDegreesResultsIpi* expected =
				fiftyoneDegreesResultsIpiCreate(&none.manager);
			populate(expected, address);
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsEvaluateAll(&expected->b, exception);
			FIFTYONE_DEGREES_EXCEPTION_THROW;
			for (const bool reverse : { false, true }) {
				std::vector<std::string> order = names;
				if (reverse) {
					std::reverse(order.begin(), order.end());
				}
				fiftyoneDegreesResultsIpi* results =
					fiftyoneDegreesResultsIpiCreate(&some.manager);
				populate(results, address);
				for (const std::string& name : order) {
					EXPECT_EQ(
						readByName(expected, name.c_str()),
						readByName(results, name.c_str())) << address <<
						", " << name << (reverse ? ", in reverse" : "");
				}
				fiftyoneDegreesResultsIpiFree(results);
			}
			fiftyoneDegreesResultsIpiFree(expected);
		}
	}
}

/**
 * Populating the results of an engine with properties named allocates
 * nothing when the data set is in memory, as with none named.
 */
TEST_F(IpiFirstReadRuleTests, Named_PopulatingAllocatesNothing) {
	ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
		"' was not found.";
	const std::string list = join(onePerComponent(propertiesOf(smallFilePath)));
	for (const bool withNames : { false, true }) {
		Tracking tracking;
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			withNames ? named(list.c_str()) : fiftyoneDegreesPropertiesDefault,
			fiftyoneDegreesIpiInMemoryConfig,
			smallFilePath));
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		startCounting(0);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			addresses[0],
			strlen(addresses[0]),
			exception);
		stopCounting();
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		EXPECT_EQ(0, mallocCalls) << "Populating the results allocated, " <<
			(withNames ? "named" : "none named");
		fiftyoneDegreesResultsIpiFree(results);
	}
}

// ---------------------------------------------------------------------------
// Unhappy paths.
// ---------------------------------------------------------------------------

/**
 * With properties named, a graph that cannot be read fails populating the
 * results, which reports it.
 */
TEST_F(IpiFirstReadRuleTests, Named_GraphCannotBeRead_ProcessReportsIt) {
	ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
		"' was not found.";
	const std::string list = join(onePerComponent(propertiesOf(smallFilePath)));
	Manager m;
	ASSERT_NO_FATAL_FAILURE(initWithGraphsCut(m, named(list.c_str())));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		addresses[0],
		strlen(addresses[0]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED) <<
		"Populating the results did not report the failed graph read.";
	fiftyoneDegreesResultsIpiFree(results);
	m.release();
	removeCopy();
}

/**
 * With no properties named, populating the results reads no graph, the first
 * read of a property reports the failed graph read, and the component is not
 * walked again for the results.
 */
TEST_F(IpiFirstReadRuleTests, NoneNamed_GraphCannotBeRead_FirstReadReportsIt) {
	Manager m;
	ASSERT_NO_FATAL_FAILURE(
		initWithGraphsCut(m, fiftyoneDegreesPropertiesDefault));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		addresses[0],
		strlen(addresses[0]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"Populating the results read a graph.";
	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	fiftyoneDegreesResultsIpiGetValues(results, 0, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED) <<
		"The first read did not report the failed graph read.";
	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	fiftyoneDegreesResultsIpiGetHasValues(results, 0, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"A later read walked the graph again.";
	fiftyoneDegreesResultsIpiFree(results);
	m.release();
	removeCopy();
}

/**
 * With properties named, every allocation of creating, populating and
 * reading the results failing in turn is reported and leaves nothing.
 */
TEST_F(IpiFirstReadRuleTests, Named_EveryAllocationFails_IsReported) {
	ASSERT_FALSE(smallFilePath.empty()) << "The file '" << smallFileName <<
		"' was not found.";
	const std::string list = join(onePerComponent(propertiesOf(smallFilePath)));
	expectEveryAllocationFailureReported(named(list.c_str()));
}

/**
 * The same with no properties named, where the first reads walk the graphs.
 */
TEST_F(IpiFirstReadRuleTests, NoneNamed_EveryAllocationFails_IsReported) {
	expectEveryAllocationFailureReported(fiftyoneDegreesPropertiesDefault);
}

/**
 * Evidence with no address, and no evidence, give no results and walk no
 * graph, with properties named and with none.
 */
TEST_F(IpiFirstReadRuleTests, EvidenceForNoComponent_WalksNoGraph) {
	const std::string list = join(onePerComponent(propertiesOf(dataFilePath)));
	for (const bool withNames : { false, true }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			withNames ? named(list.c_str()) : fiftyoneDegreesPropertiesDefault));
		for (const bool withPair : { false, true }) {
			fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
				fiftyoneDegreesEvidenceCreate(1);
			if (withPair) {
				fiftyoneDegreesEvidenceAddString(
					evidence,
					FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
					"User-Agent",
					"Mozilla/5.0");
			}
			fiftyoneDegreesResultsIpi* results =
				fiftyoneDegreesResultsIpiCreate(&m.manager);
			std::vector<int> walks;
			countWalks(results, walks);
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			fiftyoneDegreesResultsIpiFromEvidence(
				results,
				evidence,
				exception);
			FIFTYONE_DEGREES_EXCEPTION_THROW;
			EXPECT_EQ(0u, results->count);
			EXPECT_TRUE(walks.empty()) << "A graph was walked, " <<
				(withNames ? "named" : "none named");
			describeAll(results);
			EXPECT_TRUE(walks.empty()) << "A read walked a graph, " <<
				(withNames ? "named" : "none named");
			fiftyoneDegreesResultsIpiFree(results);
			fiftyoneDegreesEvidenceFree(evidence);
		}
	}
}

/**
 * A property the data set does not have reads as missing, by name and by
 * position, with properties named and with none, and walks no graph.
 */
TEST_F(IpiFirstReadRuleTests, UnavailableProperty_ReadsAsMissing) {
	const std::vector<std::string> names =
		twoNames(propertiesOf(dataFilePath));
	ASSERT_FALSE(names.empty());
	for (const bool withNames : { false, true }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(init(
			m,
			withNames ?
				named(names[0].c_str()) : fiftyoneDegreesPropertiesDefault));
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		populate(results, addresses[0]);
		std::vector<int> walks;
		countWalks(results, walks);
		const std::string missing = withNames && names.size() > 1 ?
			names[1] : std::string("NoSuchProperty");
		std::vector<char> buffer(valueBufferBytes, '\0');
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiGetValuesString(
			results,
			missing.c_str(),
			buffer.data(),
			buffer.size(),
			"|",
			exception);
		EXPECT_EQ(std::string(), std::string(buffer.data())) << missing;
		FIFTYONE_DEGREES_EXCEPTION_CLEAR;
		const int beyond = positionCount(results);
		EXPECT_FALSE(fiftyoneDegreesResultsIpiGetHasValues(
			results,
			beyond,
			exception));
		EXPECT_EQ(
			FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
			fiftyoneDegreesResultsIpiGetNoValueReason(
				results,
				beyond,
				exception));
		FIFTYONE_DEGREES_EXCEPTION_CLEAR;
		EXPECT_TRUE(walks.empty()) <<
			"Reading a property the data set does not have walked a graph, " <<
			(withNames ? "named" : "none named");
		fiftyoneDegreesResultsIpiFree(results);
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
TEST_F(IpiFirstReadRuleTests, Reload_ReadAfterReload_AnswersFromTheHeldDataSet) {
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	const std::string list = renamedName + "," + keptName;
	for (const bool withNames : { false, true }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			m,
			good,
			withNames ? named(list.c_str()) : fiftyoneDegreesPropertiesDefault));
		fiftyoneDegreesResultsIpi* expected =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		fiftyoneDegreesResultsIpi* held =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		populate(expected, addresses[0]);
		populate(held, addresses[0]);
		const std::vector<std::string> before = describeAll(expected);
		fiftyoneDegreesResultsIpiFree(expected);

		ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
		EXPECT_EQ(before, describeAll(held)) <<
			(withNames ? "named" : "none named");

		fiftyoneDegreesResultsIpi* reloaded =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		populate(reloaded, addresses[0]);
		EXPECT_NE(before, describeAll(reloaded)) <<
			"The reload did not change what the positions read, " <<
			(withNames ? "named" : "none named");
		fiftyoneDegreesResultsIpiFree(reloaded);
		fiftyoneDegreesResultsIpiFree(held);
	}
}

/**
 * Results created before a reload and populated again after it keep the data
 * set they were created with, as they hold it until they are freed, with
 * properties named and with none, while results created after the reload
 * use the reloaded data set.
 */
TEST_F(IpiFirstReadRuleTests, Reload_ReusedResults_KeepTheirDataSet) {
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	const std::string list = renamedName + "," + keptName;
	for (const bool withNames : { false, true }) {
		Manager m;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			m,
			good,
			withNames ? named(list.c_str()) : fiftyoneDegreesPropertiesDefault));
		fiftyoneDegreesResultsIpi* reused =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		populate(reused, addresses[0]);
		const std::vector<std::string> before = describeAll(reused);
		const void* held = reused->b.dataSet;

		ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
		populate(reused, addresses[0]);
		EXPECT_EQ(held, reused->b.dataSet);
		EXPECT_EQ(before, describeAll(reused)) <<
			(withNames ? "named" : "none named");

		fiftyoneDegreesResultsIpi* fresh =
			fiftyoneDegreesResultsIpiCreate(&m.manager);
		EXPECT_NE(held, fresh->b.dataSet);
		populate(fresh, addresses[0]);
		EXPECT_NE(before, describeAll(fresh)) <<
			(withNames ? "named" : "none named");
		fiftyoneDegreesResultsIpiFree(fresh);
		fiftyoneDegreesResultsIpiFree(reused);
	}
}

/**
 * A reload to a file that lacks a named property keeps the property's
 * position, which reads as a property the data set does not have, and
 * populating the results walks the graphs of the named properties the file
 * has, without failing, giving what an engine naming only those gives.
 */
TEST_F(IpiFirstReadRuleTests, Reload_NamedPropertyMissing_ProcessSkipsIt) {
	ASSERT_NO_FATAL_FAILURE(renameFirstProperty());
	const std::string list = renamedName + "," + keptName;
	Manager m;
	ASSERT_NO_FATAL_FAILURE(initFromMemory(m, good, named(list.c_str())));
	ASSERT_NO_FATAL_FAILURE(reload(m, renamed));
	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&m.manager);
	std::vector<int> walks;
	countWalks(results, walks);
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		addresses[0],
		strlen(addresses[0]),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"Populating the results failed for a named property the file lacks.";
	EXPECT_EQ(availableComponents(results), walks);
	EXPECT_FALSE(anyPending(results));
	EXPECT_EQ(2, positionCount(results));
	EXPECT_FALSE(fiftyoneDegreesResultsIpiGetHasValues(results, 0, exception));
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_INVALID_PROPERTY,
		fiftyoneDegreesResultsIpiGetNoValueReason(results, 0, exception));
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);

	Manager only;
	ASSERT_NO_FATAL_FAILURE(initFromMemory(
		only,
		renamed,
		named(keptName.c_str())));
	fiftyoneDegreesResultsIpi* expected =
		fiftyoneDegreesResultsIpiCreate(&only.manager);
	populate(expected, addresses[0]);
	EXPECT_EQ(describe(expected, 0), describe(results, 1)) <<
		"The named property the file has read differently.";
	fiftyoneDegreesResultsIpiFree(expected);
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * Reloads made while other threads populate and read their own results
 * neither fail a read nor crash, with no properties named.
 */
TEST_F(IpiFirstReadRuleTests, NoneNamed_ReloadWhileReading_IsSafe) {
	expectReloadsWhileReadingSafe(false);
}

/**
 * The same with properties named.
 */
TEST_F(IpiFirstReadRuleTests, Named_ReloadWhileReading_IsSafe) {
	expectReloadsWhileReadingSafe(true);
}
