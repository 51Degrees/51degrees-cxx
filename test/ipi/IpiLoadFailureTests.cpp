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
 * @file IpiLoadFailureTests.cpp
 * @brief Makes each allocation of a load and of a reload fail in turn, with an
 * exception and with none. The status must show the failure, a reload must
 * keep the data set in use, and nothing may be left allocated.
 */

#include "../common/Base.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/ipi.h"

/** The data file committed to the data submodule, so every build has it. */
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** The allocations made so far by countingMalloc. */
static int mallocCalls = 0;

/** The allocation that fails, counting from one, or zero for none. */
static int mallocFailsOn = 0;

static void* FIFTYONE_DEGREES_CALL_CONV countingMalloc(size_t size) {
	return ++mallocCalls == mallocFailsOn ?
		nullptr : fiftyoneDegreesMemoryTrackingMalloc(size);
}

class IpiLoadFailureTests : public Base {
protected:
	std::vector<unsigned char> data;
	std::string path;

	void SetUp() override {
		Base::SetUp();
		const std::string found = GetFilePath(_dataFolderName, asnFileName);
		if (found.empty()) {
			GTEST_SKIP() << "The file '" << asnFileName << "' was not found. "
				"Run 'git submodule update --init' to fetch it.";
		}
		FILE *file = fopen(found.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << found << "'.";
		fseek(file, 0, SEEK_END);
		data.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), bytes) << "Could not read '" << found << "'.";
		path = std::string("ipi-load-failure-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".ipi";
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(path.c_str(), data.data(), data.size()));
	}

	void TearDown() override {
		remove(path.c_str());
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(data);
		std::string().swap(path);
		Base::TearDown();
	}

	static void startCounting(int failsOn) {
		mallocCalls = 0;
		mallocFailsOn = failsOn;
		fiftyoneDegreesMalloc = countingMalloc;
	}

	static void stopCounting() {
		fiftyoneDegreesMalloc = fiftyoneDegreesMemoryTrackingMalloc;
	}

	/**
	 * Loads from memory, or from the file, with the allocation failing. A
	 * manager that loaded is freed, and the bytes left allocated are returned
	 * through the pointer.
	 */
	fiftyoneDegreesStatusCode load(
		const bool fromMemory,
		fiftyoneDegreesConfigIpi config,
		const int failsOn,
		fiftyoneDegreesException *exception,
		size_t *left) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		fiftyoneDegreesSetUpMemoryTracking();
		startCounting(failsOn);
		const fiftyoneDegreesStatusCode status = fromMemory ?
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception) :
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path.c_str(),
				exception);
		stopCounting();
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		*left = fiftyoneDegreesUnsetMemoryTracking();
		return status;
	}

	/**
	 * Expects every allocation of a load failing in turn to be shown in the
	 * status, with an exception and with none, and to leave nothing.
	 */
	void expectEveryLoadFailureReported(
		const bool fromMemory,
		const fiftyoneDegreesConfigIpi &config) {
		size_t left;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			load(fromMemory, config, 0, nullptr, &left));
		const int calls = mallocCalls;
		ASSERT_EQ((size_t)0, left);
		for (int n = 1; n <= calls; n++) {
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			EXPECT_NE(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				load(fromMemory, config, n, exception, &left)) <<
				"Allocation " << n << " of " << calls;
			EXPECT_EQ((size_t)0, left) << "Allocation " << n << " of " << calls;
			EXPECT_NE(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				load(fromMemory, config, n, nullptr, &left)) <<
				"Allocation " << n << " of " << calls << " with no exception";
			EXPECT_EQ((size_t)0, left) <<
				"Allocation " << n << " of " << calls << " with no exception";
		}
	}

	/**
	 * Expects every allocation of a reload of the original file failing in
	 * turn to be shown in the status, with an exception and with none, to
	 * keep the data set in use, and to leave nothing more allocated.
	 */
	void expectEveryReloadFailureKeepsActive(fiftyoneDegreesConfigIpi config) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		fiftyoneDegreesSetUpMemoryTracking();
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path.c_str(),
				exception));
		startCounting(0);
		const fiftyoneDegreesStatusCode counted =
			fiftyoneDegreesIpiReloadManagerFromOriginalFile(
				&manager,
				exception);
		stopCounting();
		const int calls = mallocCalls;
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, counted);
		for (int n = 1; n <= calls; n++) {
			for (const bool withException : { true, false }) {
				FIFTYONE_DEGREES_EXCEPTION_CLEAR;
				const void *active = (const void*)manager.active;
				const size_t allocated =
					fiftyoneDegreesMemoryTrackingGetAllocated();
				startCounting(n);
				const fiftyoneDegreesStatusCode status =
					fiftyoneDegreesIpiReloadManagerFromOriginalFile(
						&manager,
						withException ? exception : nullptr);
				stopCounting();
				EXPECT_NE(FIFTYONE_DEGREES_STATUS_SUCCESS, status) <<
					"Allocation " << n << " of " << calls <<
					(withException ? "" : " with no exception");
				EXPECT_EQ(active, (const void*)manager.active) <<
					"Allocation " << n << " of " << calls <<
					(withException ? "" : " with no exception");
				EXPECT_EQ(
					allocated,
					fiftyoneDegreesMemoryTrackingGetAllocated()) <<
					"Allocation " << n << " of " << calls <<
					(withException ? "" : " with no exception");
			}
		}
		fiftyoneDegreesResourceManagerFree(&manager);
		EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
	}
};

/**
 * Test a load from memory, which creates the headers, the component list and
 * the components available.
 */
TEST_F(IpiLoadFailureTests, FromMemory_EveryAllocationFails_IsReported) {
	expectEveryLoadFailureReported(true, fiftyoneDegreesIpiInMemoryConfig);
}

/**
 * Test a load from a file in a profile that caches what it reads.
 */
TEST_F(IpiLoadFailureTests, FromFile_Balanced_EveryAllocationFails_IsReported) {
	expectEveryLoadFailureReported(false, fiftyoneDegreesIpiBalancedConfig);
}

/**
 * Test a load from a file in a profile that reads each item from the file
 * when it is needed, so a failed allocation can leave a name unread.
 */
TEST_F(
	IpiLoadFailureTests,
	FromFile_LowMemory_EveryAllocationFails_IsReported) {
	expectEveryLoadFailureReported(false, fiftyoneDegreesIpiLowMemoryConfig);
}

/**
 * Test a reload from a file in a profile that caches what it reads.
 */
TEST_F(
	IpiLoadFailureTests,
	Reload_Balanced_EveryAllocationFails_KeepsActive) {
	expectEveryReloadFailureKeepsActive(fiftyoneDegreesIpiBalancedConfig);
}
