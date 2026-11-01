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
 * @file HashSizeManagerFromMemoryTests.cpp
 * @brief Tests that the size method from memory reports the status of memory
 * that cannot load, that the init method it calls frees what it allocated on
 * the way to failing, and that the memory stays with the caller.
 *
 * Each test builds the memory it offers, so no data file is needed.
 */

#include "../../common/pch.h"
#include <cstddef>
#include <cstdint>
#include "../../common/Base.hpp"
#include "../../common/TestUtils_SizeManager.hpp"
#include "HashTestMemory.hpp"

class HashSizeManagerFromMemoryTests : public Base {
protected:
	typedef TestHeader Header;
	typedef TestMemory Memory;

	/**
	 * Gets the size and the status the size method leaves in the exception,
	 * with a required property list that may be null.
	 */
	static size_t sizeWithConfig(
		fiftyoneDegreesConfigHash *config,
		Memory &memory,
		const char *required,
		fiftyoneDegreesStatusCode *status) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		properties.string = required;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		TimeLimit limit(SizeMethodLimitSeconds);
		fillStackWithZeroedHandles();
		const size_t size = fiftyoneDegreesHashSizeManagerFromMemory(
			config,
			&properties,
			memory.data(),
			(long)memory.size(),
			exception);
		*status = exception->status;
		return size;
	}

	/**
	 * Gets the status the size method leaves in the exception. The stack is
	 * filled last, so that nothing between that and the call overwrites it.
	 */
	static fiftyoneDegreesStatusCode sizeStatus(
		void *memory,
		long size,
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		TimeLimit limit(SizeMethodLimitSeconds);
		fillStackWithZeroedHandles();
		fiftyoneDegreesHashSizeManagerFromMemory(
			&config,
			&properties,
			memory,
			size,
			exception);
		return exception->status;
	}

	static fiftyoneDegreesStatusCode sizeStatus(Memory &memory) {
		return sizeStatus(memory.data(), (long)memory.size());
	}

	/**
	 * Gets the status from the init method and the bytes it leaves allocated.
	 * Only for memory the init method refuses, as no manager is freed here.
	 */
	static fiftyoneDegreesStatusCode initTracked(
		Memory &memory,
		bool freeData,
		size_t *allocated) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		config.b.b.freeData = freeData;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesSetUpMemoryTracking();
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				memory.data(),
				(long)memory.size(),
				exception);
		*allocated = fiftyoneDegreesUnsetMemoryTracking();
		return status;
	}
};

/**
 * Test that no memory at all is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, NoMemory_ReportsNullPointer) {
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NULL_POINTER, sizeStatus(nullptr, 0));
}

/**
 * Test that a size of zero is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, NoBytes_ReportsCorruptData) {
	Memory memory(1, 1);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		sizeStatus(memory.data(), 0));
}

/**
 * Test that memory far too short to hold a header is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, ThreeBytes_ReportsCorruptData) {
	Memory memory(3, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that memory one byte too short to hold a header is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, OneByteShort_ReportsCorruptData) {
	Memory memory(sizeof(Header) - 1, 1);
	setVersion(memory);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a negative size, which puts the end of the memory before its
 * start, is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, NegativeSize_ReportsCorruptData) {
	Memory memory(1, 1);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		sizeStatus(memory.data(), -1));
}

/**
 * Test that memory holding garbage is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, Garbage_ReportsIncorrectVersion) {
	Memory memory = garbage(64 * 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION, sizeStatus(memory));
}

/**
 * Test that garbage after the version the engine reads is reported.
 */
TEST_F(HashSizeManagerFromMemoryTests, GarbageWithVersion_ReportsCorruptData) {
	Memory memory = garbage(64 * 1024);
	setVersion(memory);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a collection that does not start where the one before it ended is
 * reported, after collections have been allocated.
 */
TEST_F(HashSizeManagerFromMemoryTests, CollectionOutOfStep_ReportsCorruptData) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, maps), sizeof(Header) + 1, 0);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a collection longer than the memory is reported, after collections
 * have been allocated.
 */
TEST_F(HashSizeManagerFromMemoryTests, CollectionTooLong_ReportsCorruptData) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, values), sizeof(Header), 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that bytes left over after the last collection are reported, after all
 * the collections have been allocated.
 */
TEST_F(HashSizeManagerFromMemoryTests, BytesLeftOver_ReportsOutOfBounds) {
	Memory memory = emptyDataSet();
	memory.resize(memory.size() + 16);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_POINTER_OUT_OF_BOUNDS,
		sizeStatus(memory));
}

/**
 * Test that a failure only the exception shows is left in the exception. The
 * init method returns success for this one.
 */
TEST_F(HashSizeManagerFromMemoryTests, ComponentMissing_ReportsOutOfRange) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, components), sizeof(Header), 0, 1);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_COLLECTION_OFFSET_OUT_OF_RANGE,
		sizeStatus(memory));
}

/**
 * Test that a data set with no properties is reported, which is the last
 * failure reached without a data file.
 */
TEST_F(HashSizeManagerFromMemoryTests, NoProperties_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		sizeStatus(memory));
}

/**
 * Test that a null configuration, which the comments on the method allow, gets
 * as far as reporting the status.
 */
TEST_F(HashSizeManagerFromMemoryTests, NullConfig_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	fiftyoneDegreesStatusCode status;
	sizeWithConfig(nullptr, memory, nullptr, &status);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT, status);
}

/**
 * Test that a null configuration means the balanced one for memory that loads.
 */
TEST_F(HashSizeManagerFromMemoryTests, NullConfig_SameSizeAsBalanced) {
	Memory memory = dataSetWithComponents(2);
	fiftyoneDegreesStatusCode status;
	const size_t size = sizeWithConfig(nullptr, memory, "DeviceId", &status);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NOT_SET, status);
	const size_t balanced = sizeWithConfig(
		&fiftyoneDegreesHashBalancedConfig,
		memory,
		"DeviceId",
		&status);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NOT_SET, status);
	EXPECT_GT(size, (size_t)0);
	EXPECT_EQ(balanced, size);
}

/**
 * Test that every configuration reports the status, including the one with a
 * temporary file, which freeing a data set tries to delete.
 */
TEST_F(HashSizeManagerFromMemoryTests, EveryConfig_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	const fiftyoneDegreesConfigHash * const configs[] = {
		&fiftyoneDegreesHashInMemoryConfig,
		&fiftyoneDegreesHashHighPerformanceConfig,
		&fiftyoneDegreesHashLowMemoryConfig,
		&fiftyoneDegreesHashBalancedConfig,
		&fiftyoneDegreesHashBalancedTempConfig };
	for (const fiftyoneDegreesConfigHash * const config : configs) {
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
			sizeStatus(memory.data(), (long)memory.size(), *config));
	}
}

/**
 * Test that a configuration with no graphs is reported, which the init method
 * refuses before it allocates anything.
 */
TEST_F(HashSizeManagerFromMemoryTests, NoGraphs_ReportsInvalidConfig) {
	Memory memory = emptyDataSet();
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	config.usePerformanceGraph = false;
	config.usePredictiveGraph = false;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INVALID_CONFIG,
		sizeStatus(memory.data(), (long)memory.size(), config));
}

/**
 * Test that a caller who passes no exception gets through a failure, and that
 * the allocation methods are back to the standard ones afterwards.
 */
TEST_F(HashSizeManagerFromMemoryTests, NoException_RestoresAllocators) {
	Memory memory = garbage(64 * 1024);
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	{
		TimeLimit limit(SizeMethodLimitSeconds);
		fillStackWithZeroedHandles();
		fiftyoneDegreesHashSizeManagerFromMemory(
			&config,
			&properties,
			memory.data(),
			(long)memory.size(),
			nullptr);
	}
	EXPECT_TRUE(fiftyoneDegreesMalloc == fiftyoneDegreesMemoryStandardMalloc);
	EXPECT_TRUE(fiftyoneDegreesMallocAligned ==
		fiftyoneDegreesMemoryStandardMallocAligned);
	EXPECT_TRUE(fiftyoneDegreesFree == fiftyoneDegreesMemoryStandardFree);
	EXPECT_TRUE(fiftyoneDegreesFreeAligned ==
		fiftyoneDegreesMemoryStandardFreeAligned);
}

/**
 * Test that the size method leaves the memory with the caller on failure when
 * the configuration says to free it. The memory is freed when the test ends,
 * which fails if the method has freed it already.
 */
TEST_F(HashSizeManagerFromMemoryTests, FreeData_SizeLeavesMemoryWithCaller) {
	Memory memory = emptyDataSet();
	const Memory before = memory;
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	config.b.b.freeData = true;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		sizeStatus(memory.data(), (long)memory.size(), config));
	EXPECT_EQ(before, memory);
}

/**
 * Test that the init method leaves the memory with the caller on failure when
 * the configuration says to free it. The tracking free method fails on memory
 * it did not allocate, so freeing the memory here does not go unseen.
 */
TEST_F(HashSizeManagerFromMemoryTests, FreeData_InitLeavesMemoryWithCaller) {
	Memory memory = emptyDataSet();
	const Memory before = memory;
	size_t allocated;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		initTracked(memory, true, &allocated));
	EXPECT_EQ((size_t)0, allocated);
	EXPECT_EQ(before, memory);
}

/**
 * Test that the init method frees the collections allocated before a
 * collection it could not read.
 */
TEST_F(HashSizeManagerFromMemoryTests, Init_CollectionFault_FreesAllocated) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, maps), sizeof(Header) + 1, 0);
	size_t allocated;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		initTracked(memory, false, &allocated));
	EXPECT_EQ((size_t)0, allocated);
}

/**
 * Test that the init method frees what it allocated before a failure only the
 * exception shows.
 */
TEST_F(HashSizeManagerFromMemoryTests, Init_ComponentMissing_FreesAllocated) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, components), sizeof(Header), 0, 1);
	size_t allocated;
	initTracked(memory, false, &allocated);
	EXPECT_EQ((size_t)0, allocated);
}

/**
 * Test that the init method frees the collections, lists and properties
 * allocated before finding there are no properties.
 */
TEST_F(HashSizeManagerFromMemoryTests, Init_NoProperties_FreesAllocated) {
	Memory memory = emptyDataSet();
	size_t allocated;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		initTracked(memory, false, &allocated));
	EXPECT_EQ((size_t)0, allocated);
}
