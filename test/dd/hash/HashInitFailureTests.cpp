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
 * @file HashInitFailureTests.cpp
 * @brief Tests the init methods and the size method from a file with a null
 * configuration, a null exception, a failure only the exception shows, and a
 * failed allocation at the end of the init method.
 *
 * Each test builds the data it offers, so no data file is needed.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <string>
#include "../../common/Base.hpp"
#include "../../common/TestUtils_SizeManager.hpp"
#include "HashTestMemory.hpp"

static const char missingFile[] = "51Degrees-NoSuchFile.hash";

/** The components in the data set the allocation tests load. */
static const uint32_t componentCount = 2;

/** The allocations made so far by countingMalloc. */
static int mallocCalls = 0;

/** The allocation that fails, counting from one, or zero for none. */
static int mallocFailsOn = 0;

static void* FIFTYONE_DEGREES_CALL_CONV countingMalloc(size_t size) {
	return ++mallocCalls == mallocFailsOn ?
		nullptr : fiftyoneDegreesMemoryTrackingMalloc(size);
}

class HashInitFailureTests : public Base {
protected:
	std::string fileName;

	void SetUp() override {
		Base::SetUp();
		fileName = std::string("hash-init-failure-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".tmp";
	}

	void TearDown() override {
		remove(fileName.c_str());
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(fileName);
		Base::TearDown();
	}

	void writeFile(const TestMemory &memory) {
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(
				fileName.c_str(),
				memory.data(),
				memory.size()));
	}

	/**
	 * Gets a configuration that loads the data set into memory with no index
	 * of property values, which the allocation tests' data set could not fill.
	 */
	static fiftyoneDegreesConfigHash loadableConfig() {
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		config.b.b.propertyValueIndex = false;
		return config;
	}

	/**
	 * Gets the status from the init method from memory, or from the file if
	 * there is no memory, with the one allocation failing. The exception may
	 * be null. A manager that was set up is freed, and the bytes left allocated
	 * are returned through the pointer.
	 */
	fiftyoneDegreesStatusCode initWithFailure(
		TestMemory *memory,
		int failsOn,
		fiftyoneDegreesException *exception,
		size_t *allocated,
		int *calls = nullptr) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesConfigHash config = loadableConfig();
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = "DeviceId";
		fiftyoneDegreesSetUpMemoryTracking();
		fiftyoneDegreesMalloc = countingMalloc;
		mallocCalls = 0;
		mallocFailsOn = failsOn;
		const fiftyoneDegreesStatusCode status = memory != nullptr ?
			fiftyoneDegreesHashInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				memory->data(),
				(long)memory->size(),
				exception) :
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				fileName.c_str(),
				exception);
		if (calls != nullptr) {
			*calls = mallocCalls;
		}
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS &&
			FIFTYONE_DEGREES_EXCEPTION_OKAY) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		*allocated = fiftyoneDegreesUnsetMemoryTracking();
		return status;
	}

	/**
	 * Expects the status for the allocation counted back from the last one in
	 * the init method, both with and without an exception to hold it, and that
	 * nothing is left allocated.
	 */
	void expectAllocationFailure(TestMemory *memory, int fromLast) {
		size_t allocated;
		int calls = 0;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			initWithFailure(memory, 0, nullptr, &allocated, &calls));
		ASSERT_GT(calls, fromLast);
		const int failsOn = calls - fromLast;

		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
			initWithFailure(memory, failsOn, nullptr, &allocated)) <<
			"With a null exception";
		EXPECT_EQ((size_t)0, allocated);

		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
			initWithFailure(memory, failsOn, exception, &allocated)) <<
			"With an exception";
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
			exception->status);
		EXPECT_EQ((size_t)0, allocated);
	}

	/**
	 * Expects the status for the allocation counted back from the last one,
	 * from memory and from a file.
	 */
	void expectAllocationFailure(int fromLast) {
		TestMemory memory = dataSetWithComponents(componentCount);
		expectAllocationFailure(&memory, fromLast);
		ASSERT_NO_FATAL_FAILURE(writeFile(memory));
		expectAllocationFailure((TestMemory*)nullptr, fromLast);
	}
};

/**
 * Test that a caller who passes no exception gets through a file that is not
 * there in every configuration, and that the allocation methods are back to the
 * standard ones.
 */
TEST_F(HashInitFailureTests, SizeFromFile_NoException_RestoresAllocators) {
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	const fiftyoneDegreesConfigHash * const configs[] = {
		&fiftyoneDegreesHashInMemoryConfig,
		&fiftyoneDegreesHashHighPerformanceConfig,
		&fiftyoneDegreesHashLowMemoryConfig,
		&fiftyoneDegreesHashBalancedConfig,
		&fiftyoneDegreesHashBalancedTempConfig };
	for (const fiftyoneDegreesConfigHash * const source : configs) {
		fiftyoneDegreesConfigHash config = *source;
		{
			TimeLimit limit(SizeMethodLimitSeconds);
			fillStackWithZeroedHandles();
			fiftyoneDegreesHashSizeManagerFromFile(
				&config,
				&properties,
				missingFile,
				nullptr);
		}
		EXPECT_TRUE(
			fiftyoneDegreesMalloc == fiftyoneDegreesMemoryStandardMalloc);
		EXPECT_TRUE(
			fiftyoneDegreesFree == fiftyoneDegreesMemoryStandardFree);
	}
}

/**
 * Test that the size method from a file does not free a manager when only the
 * exception shows the failure, as the init method returns success with no
 * manager set up.
 */
TEST_F(HashInitFailureTests, SizeFromFile_ComponentMissing_ReportsOutOfRange) {
	TestMemory memory = emptyDataSet();
	setCollection(
		memory,
		offsetof(TestHeader, components),
		sizeof(TestHeader),
		0,
		1);
	ASSERT_NO_FATAL_FAILURE(writeFile(memory));
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	TimeLimit limit(SizeMethodLimitSeconds);
	fillStackWithZeroedHandles();
	fiftyoneDegreesHashSizeManagerFromFile(
		&config,
		&properties,
		fileName.c_str(),
		exception);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_COLLECTION_OFFSET_OUT_OF_RANGE,
		exception->status);
}

/**
 * Test that a null configuration, which the comments on the method allow, gets
 * as far as reporting the status.
 */
TEST_F(HashInitFailureTests, SizeFromFile_NullConfig_ReportsFileNotFound) {
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	TimeLimit limit(SizeMethodLimitSeconds);
	fillStackWithZeroedHandles();
	fiftyoneDegreesHashSizeManagerFromFile(
		nullptr,
		&properties,
		missingFile,
		exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND, exception->status);
}

/**
 * Test that a null configuration, which the comments on the method allow, gets
 * as far as returning the status.
 */
TEST_F(HashInitFailureTests, InitFromFile_NullConfig_ReturnsFileNotFound) {
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND,
		fiftyoneDegreesHashInitManagerFromFile(
			&manager,
			nullptr,
			&properties,
			missingFile,
			exception));
}

/**
 * Test that a null configuration, which the comments on the method allow, gets
 * as far as returning the status.
 */
TEST_F(HashInitFailureTests, InitFromMemory_NullConfig_ReturnsNotPresent) {
	TestMemory memory = emptyDataSet();
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		fiftyoneDegreesHashInitManagerFromMemory(
			&manager,
			nullptr,
			&properties,
			memory.data(),
			(long)memory.size(),
			exception));
}

/**
 * Test that a null configuration means the balanced one for memory that loads,
 * through to a manager that frees everything.
 */
TEST_F(HashInitFailureTests, InitFromMemory_NullConfig_Loads) {
	TestMemory memory = dataSetWithComponents(componentCount);
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = "DeviceId";
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesSetUpMemoryTracking();
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesHashInitManagerFromMemory(
			&manager,
			nullptr,
			&properties,
			memory.data(),
			(long)memory.size(),
			exception);
	if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
		fiftyoneDegreesResourceManagerFree(&manager);
	}
	const size_t allocated = fiftyoneDegreesUnsetMemoryTracking();
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NOT_SET, exception->status);
	EXPECT_EQ((size_t)0, allocated);
}

/**
 * Test that failing to allocate the array of component headers is reported.
 * The array and then one entry for each component are the last allocations.
 */
TEST_F(HashInitFailureTests, ComponentHeadersArray_NoMemory_ReportsStatus) {
	expectAllocationFailure((int)componentCount);
}

/**
 * Test that failing to allocate the first component's headers is reported, with
 * the rest of the array not yet set.
 */
TEST_F(HashInitFailureTests, FirstComponentHeaders_NoMemory_ReportsStatus) {
	expectAllocationFailure((int)componentCount - 1);
}

/**
 * Test that failing to allocate the last component's headers is reported, and
 * the headers already allocated are freed.
 */
TEST_F(HashInitFailureTests, LastComponentHeaders_NoMemory_ReportsStatus) {
	expectAllocationFailure(0);
}
