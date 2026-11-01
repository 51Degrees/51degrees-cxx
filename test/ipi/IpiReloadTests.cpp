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
 * @file IpiReloadTests.cpp
 * @brief Tests a reload through each route a caller can take, in every
 * performance profile. Good data must replace the active data set. Broken data
 * must report the failure, keep the active data set and free the new one.
 *
 * The broken data is the data file with one more component in its header than
 * the file holds. The init methods return success for it and only the
 * exception shows the failure.
 */

#include "../common/Base.hpp"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/ipi.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

/** The data file committed to the data submodule, so every build has it. */
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** An address the data file has values for, which every lookup uses. */
static const char lookupAddress[] = "1.1.1.1";

/** A property the data file has. */
static const char namedProperty[] = "AsnName";

/** How long a reload is given before the test is ended. */
static const int reloadLimitSeconds = 120;

namespace {

/**
 * Ends the process with a failure if this is still in scope after the limit,
 * as a reload that never returns cannot be stopped from its own thread.
 */
class ReloadLimit {
public:
	ReloadLimit() : watcher([this]() {
		std::unique_lock<std::mutex> lock(mutex);
		if (changed.wait_for(
			lock,
			std::chrono::seconds(reloadLimitSeconds),
			[this]() { return finished; }) == false) {
			fprintf(
				stderr,
				"Still reloading after %d seconds, so ending the test.\n",
				reloadLimitSeconds);
			fflush(stderr);
			std::_Exit(EXIT_FAILURE);
		}
	}) {
	}

	~ReloadLimit() {
		{
			std::lock_guard<std::mutex> lock(mutex);
			finished = true;
		}
		changed.notify_one();
		watcher.join();
	}

private:
	std::mutex mutex;
	std::condition_variable changed;
	bool finished = false;
	std::thread watcher; // Last, as it uses the members above once started.
};

/** The ways a caller can reload a manager or an engine. */
enum class Route { File, OriginalFile, Memory };

}

class IpiReloadTests : public Base {
protected:
	std::vector<unsigned char> good;
	std::vector<unsigned char> broken;
	std::string masterPath;
	std::string otherPath;
	fiftyoneDegreesResourceManager manager;

	void SetUp() override {
		Base::SetUp();
		const std::string path = GetFilePath(_dataFolderName, asnFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << asnFileName << "' was not found. "
				"Run 'git submodule update --init' to fetch it.";
		}
		FILE *file = fopen(path.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
		fseek(file, 0, SEEK_END);
		good.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(good.data(), 1, good.size(), file);
		fclose(file);
		ASSERT_EQ(good.size(), bytes) << "Could not read '" << path << "'.";

		broken = good;
		fiftyoneDegreesCollectionHeader components;
		const size_t position =
			offsetof(fiftyoneDegreesDataSetIpiHeader, components);
		memcpy(&components, broken.data() + position, sizeof(components));
		components.count++;
		memcpy(broken.data() + position, &components, sizeof(components));

		const std::string name =
			::testing::UnitTest::GetInstance()->current_test_info()->name();
		masterPath = "ipi-reload-" + name + ".ipi";
		otherPath = "ipi-reload-other-" + name + ".ipi";
		ASSERT_NO_FATAL_FAILURE(writeFile(masterPath, good));
	}

	void TearDown() override {
		remove(masterPath.c_str());
		remove(otherPath.c_str());
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(good);
		std::vector<unsigned char>().swap(broken);
		std::string().swap(masterPath);
		std::string().swap(otherPath);
		Base::TearDown();
	}

	static void writeFile(
		const std::string &path,
		const std::vector<unsigned char> &data) {
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(path.c_str(), data.data(), data.size()));
	}

	void initManager(
		fiftyoneDegreesConfigIpi config,
		const char *required) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				masterPath.c_str(),
				exception));
		ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	}

	/**
	 * Reloads the manager from the data through the route, placing the data
	 * where that route reads it.
	 */
	fiftyoneDegreesStatusCode reload(
		const Route route,
		std::vector<unsigned char> &data,
		fiftyoneDegreesException *exception) {
		switch (route) {
		case Route::File:
			writeFile(otherPath, data);
			return fiftyoneDegreesIpiReloadManagerFromFile(
				&manager,
				otherPath.c_str(),
				exception);
		case Route::OriginalFile:
			writeFile(masterPath, data);
			return fiftyoneDegreesIpiReloadManagerFromOriginalFile(
				&manager,
				exception);
		default:
			return fiftyoneDegreesIpiReloadManagerFromMemory(
				&manager,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception);
		}
	}

	/** Gets the values of every available property, one line per property. */
	std::string lookup() {
		char buffer[4096];
		std::string values;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi *results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			lookupAddress,
			sizeof(lookupAddress) - 1,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		const fiftyoneDegreesDataSetIpi *dataSet =
			(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
		for (int i = 0; i < (int)dataSet->b.b.available->count; i++) {
			buffer[0] = '\0';
			fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
				results,
				i,
				buffer,
				sizeof(buffer),
				"|",
				exception);
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
			values.append(buffer).append("\n");
		}
		fiftyoneDegreesResultsIpiFree(results);
		return values;
	}

	static void expectAnswers(const std::string &values) {
		EXPECT_NE(std::string::npos, values.find_first_not_of("|\n")) <<
			"The lookup found no values, so comparing them shows nothing.";
	}

	const void* active() const {
		return (const void*)manager.active;
	}

	void finish() {
		fiftyoneDegreesResourceManagerFree(&manager);
		EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
	}

	void expectGoodDataReplaces(
		const Route route,
		fiftyoneDegreesConfigIpi config,
		const char *required) {
		fiftyoneDegreesSetUpMemoryTracking();
		ASSERT_NO_FATAL_FAILURE(initManager(config, required));
		const std::string expected = lookup();
		expectAnswers(expected);
		const void *before = active();

		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status = reload(
			route,
			good,
			exception);

		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NOT_SET, exception->status) <<
			"Good data must not set the exception, or a reload that works "
			"would be reported as a failure.";
		EXPECT_NE(before, active());
		EXPECT_EQ(expected, lookup());
		finish();
	}

	/**
	 * Expects the active data set to be kept after the failed reload the
	 * caller made, with nothing more allocated and the same answers.
	 */
	void expectKept(
		const fiftyoneDegreesStatusCode status,
		const void *before,
		const size_t allocated,
		const std::string &expected) {
		EXPECT_NE(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
		EXPECT_EQ(before, active());
		EXPECT_EQ(allocated, fiftyoneDegreesMemoryTrackingGetAllocated());
		EXPECT_EQ(expected, lookup());
	}

	void expectBrokenDataKept(
		const Route route,
		fiftyoneDegreesConfigIpi config,
		const char *required) {
		fiftyoneDegreesSetUpMemoryTracking();
		ASSERT_NO_FATAL_FAILURE(initManager(config, required));
		const std::string expected = lookup();
		expectAnswers(expected);
		const void *before = active();
		const size_t allocated = fiftyoneDegreesMemoryTrackingGetAllocated();
		{
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			const fiftyoneDegreesStatusCode status = reload(
				route,
				broken,
				exception);
			EXPECT_EQ(exception->status, status) <<
				"The status must show what only the exception used to show.";
			expectKept(status, before, allocated, expected);
		}

		// A temporary copy of the broken data must not be left behind.
		if (config.b.useTempFile && route != Route::Memory) {
			char found[FIFTYONE_DEGREES_FILE_MAX_PATH] = "";
			EXPECT_FALSE(fiftyoneDegreesFileGetExistingTempFile(
				route == Route::File ? otherPath.c_str() : masterPath.c_str(),
				NULL,
				0,
				(fiftyoneDegreesFileOffset)
					sizeof(fiftyoneDegreesDataSetIpiHeader),
				found)) << "The failed reload left '" << found << "' behind.";
		}

		// The next update must still work.
		{
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				reload(route, good, exception));
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
			EXPECT_NE(before, active());
			EXPECT_EQ(expected, lookup());
		}
		finish();
	}

	void expectNoExceptionKept(
		const Route route,
		fiftyoneDegreesConfigIpi config) {
		fiftyoneDegreesSetUpMemoryTracking();
		ASSERT_NO_FATAL_FAILURE(initManager(config, nullptr));
		const std::string expected = lookup();
		expectAnswers(expected);
		const void *before = active();
		const size_t allocated = fiftyoneDegreesMemoryTrackingGetAllocated();
		fiftyoneDegreesStatusCode status;
		{
			ReloadLimit limit;
			status = reload(route, broken, nullptr);
		}
		expectKept(status, before, allocated, expected);
		finish();
	}

	/**
	 * An exception that was not clear before the call stops the init methods
	 * part way through, so even good data must not replace the active data set.
	 * The status comes from where the init method stopped, as the graphs it
	 * did not read leave bytes over.
	 */
	void expectUnclearExceptionKept(
		const Route route,
		const fiftyoneDegreesStatusCode held) {
		fiftyoneDegreesSetUpMemoryTracking();
		ASSERT_NO_FATAL_FAILURE(
			initManager(fiftyoneDegreesIpiInMemoryConfig, nullptr));
		const std::string expected = lookup();
		expectAnswers(expected);
		const void *before = active();
		const size_t allocated = fiftyoneDegreesMemoryTrackingGetAllocated();
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		exception->status = held;
		const fiftyoneDegreesStatusCode status = reload(
			route,
			good,
			exception);
		expectKept(status, before, allocated, expected);
		finish();
	}

	static std::string engineLookup(EngineIpi *engine) {
		std::unique_ptr<ResultsIpi> results(engine->process(lookupAddress));
		std::string values;
		for (int i = 0; i < results->getAvailableProperties(); i++) {
			auto value = results->getValuesAsWeightedStringList(i);
			if (value.hasValue()) {
				for (const auto &weighted : value.getValue()) {
					values.append(weighted.getValue()).append("|");
				}
			}
			values.append("\n");
		}
		return values;
	}

	/**
	 * Checks the route every language built on SWIG takes, being refreshData
	 * on an engine. The engine owns the copy it takes of memory, so tracking
	 * around the failed reload checks that is freed as well.
	 */
	void expectEngineKeepsData(const Route route, ConfigIpi &config) {
		// Restored as the tracking below covers the failed reload alone.
		fiftyoneDegreesUnsetMemoryTracking();
		RequiredPropertiesConfig properties;
		EngineIpi engine(masterPath.c_str(), &config, &properties);
		const std::string expected = engineLookup(&engine);
		expectAnswers(expected);
		const std::string path = engine.getDataFilePath();
		fiftyoneDegreesSetUpMemoryTracking();
		try {
			switch (route) {
			case Route::File:
				writeFile(otherPath, broken);
				engine.refreshData(otherPath.c_str());
				break;
			case Route::OriginalFile:
				writeFile(masterPath, broken);
				engine.refreshData();
				break;
			default:
				engine.refreshData(
					(void*)broken.data(),
					(fiftyoneDegreesFileOffset)broken.size());
				break;
			}
			ADD_FAILURE() << "No exception was thrown.";
		}
		catch (StatusCodeException &e) {
			EXPECT_NE(FIFTYONE_DEGREES_STATUS_SUCCESS, e.getCode());
		}
		EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
		EXPECT_EQ(path, engine.getDataFilePath());
		EXPECT_EQ(expected, engineLookup(&engine));
	}
};

/**
 * Each route with good and with broken data in a performance profile. A reload
 * takes its properties from the active data set, so each runs with every
 * property and with one named, which in some profiles decides what is held in
 * memory.
 */
#define IPI_RELOAD_ROUTE_TESTS(r, p) \
TEST_F(IpiReloadTests, r##_##p##_GoodData_Replaces) { \
	expectGoodDataReplaces( \
		Route::r, \
		fiftyoneDegreesIpi##p##Config, \
		nullptr); \
} \
TEST_F(IpiReloadTests, r##_##p##_NamedProperty_GoodData_Replaces) { \
	expectGoodDataReplaces( \
		Route::r, \
		fiftyoneDegreesIpi##p##Config, \
		namedProperty); \
} \
TEST_F(IpiReloadTests, r##_##p##_BrokenData_KeepsActive) { \
	expectBrokenDataKept( \
		Route::r, \
		fiftyoneDegreesIpi##p##Config, \
		nullptr); \
} \
TEST_F(IpiReloadTests, r##_##p##_NamedProperty_BrokenData_KeepsActive) { \
	expectBrokenDataKept( \
		Route::r, \
		fiftyoneDegreesIpi##p##Config, \
		namedProperty); \
}

#define IPI_RELOAD_PROFILE_TESTS(p) \
IPI_RELOAD_ROUTE_TESTS(File, p) \
IPI_RELOAD_ROUTE_TESTS(OriginalFile, p) \
IPI_RELOAD_ROUTE_TESTS(Memory, p)

IPI_RELOAD_PROFILE_TESTS(InMemory)
IPI_RELOAD_PROFILE_TESTS(HighPerformance)
IPI_RELOAD_PROFILE_TESTS(LowMemory)
IPI_RELOAD_PROFILE_TESTS(Balanced)
IPI_RELOAD_PROFILE_TESTS(BalancedTemp)
IPI_RELOAD_PROFILE_TESTS(Default)

/**
 * The states of the exception a caller can pass, other than a clear one, and
 * the engine's route, for a profile that reads from memory and one from a file.
 */
#define IPI_RELOAD_EXCEPTION_TESTS(r) \
TEST_F(IpiReloadTests, r##_NoException_InMemory_KeepsActive) { \
	expectNoExceptionKept(Route::r, fiftyoneDegreesIpiInMemoryConfig); \
} \
TEST_F(IpiReloadTests, r##_NoException_Balanced_KeepsActive) { \
	expectNoExceptionKept(Route::r, fiftyoneDegreesIpiBalancedConfig); \
} \
TEST_F(IpiReloadTests, r##_StaleException_KeepsActive) { \
	expectUnclearExceptionKept( \
		Route::r, \
		FIFTYONE_DEGREES_STATUS_FILE_FAILURE); \
} \
TEST_F(IpiReloadTests, r##_ZeroedException_KeepsActive) { \
	expectUnclearExceptionKept( \
		Route::r, \
		FIFTYONE_DEGREES_STATUS_SUCCESS); \
} \
TEST_F(IpiReloadTests, r##_Engine_InMemory_KeepsData) { \
	ConfigIpi config; \
	config.setMaxPerformance(); \
	expectEngineKeepsData(Route::r, config); \
} \
TEST_F(IpiReloadTests, r##_Engine_Balanced_KeepsData) { \
	ConfigIpi config; \
	config.setBalanced(); \
	expectEngineKeepsData(Route::r, config); \
}

IPI_RELOAD_EXCEPTION_TESTS(File)
IPI_RELOAD_EXCEPTION_TESTS(OriginalFile)
IPI_RELOAD_EXCEPTION_TESTS(Memory)
