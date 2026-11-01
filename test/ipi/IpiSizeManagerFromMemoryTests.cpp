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
 * @file IpiSizeManagerFromMemoryTests.cpp
 * @brief Tests that the size method from memory reports the status of memory
 * that cannot load, that the init method it calls frees what it allocated on
 * the way to failing, and that the memory stays with the caller.
 *
 * Each test builds the memory it offers, so no data file is needed.
 */

#include "pch.h"
#include <cstddef>
#include <cstdint>
#include "../common/Base.hpp"
#include "../common/TestUtils_SizeManager.hpp"
#include "IpiTestMemory.hpp"

class IpiSizeManagerFromMemoryTests : public Base {
protected:
	typedef TestHeader Header;
	typedef TestMemory Memory;

	/**
	 * Gets memory holding one graph whose nodes run past the end of the memory.
	 */
	static Memory graphTooLong() {
		typedef fiftyoneDegreesIpiCgInfo GraphInfo;
		Memory memory = emptyDataSet();
		const size_t end = sizeof(Header) + sizeof(GraphInfo);
		memory.resize(end, 0);
		setCollection(
			memory,
			offsetof(Header, graphs),
			sizeof(Header),
			(uint32_t)sizeof(GraphInfo),
			1);
		const size_t afterGraphs[] = {
			offsetof(Header, profileGroups),
			offsetof(Header, propertyTypes),
			offsetof(Header, profileOffsets)
		};
		for (const size_t collection : afterGraphs) {
			setCollection(memory, collection, end, 0);
		}
		GraphInfo info;
		memset(&info, 0, sizeof(info));
		info.nodes.collection.length = (uint32_t)end + 1;
		memcpy(memory.data() + sizeof(Header), &info, sizeof(info));
		return memory;
	}

	/**
	 * Gets the status the size method leaves in the exception. The stack is
	 * filled last, so that nothing between that and the call overwrites it.
	 */
	static fiftyoneDegreesStatusCode sizeStatus(
		void *memory,
		fiftyoneDegreesFileOffset size,
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig) {
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		TimeLimit limit(SizeMethodLimitSeconds);
		fillStackWithZeroedHandles();
		fiftyoneDegreesIpiSizeManagerFromMemory(
			&config,
			&properties,
			memory,
			size,
			exception);
		return exception->status;
	}

	static fiftyoneDegreesStatusCode sizeStatus(Memory &memory) {
		return sizeStatus(
			memory.data(),
			(fiftyoneDegreesFileOffset)memory.size());
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
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		config.b.freeData = freeData;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesSetUpMemoryTracking();
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				memory.data(),
				(fiftyoneDegreesFileOffset)memory.size(),
				exception);
		*allocated = fiftyoneDegreesUnsetMemoryTracking();
		return status;
	}
};

/**
 * Test that no memory at all is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, NoMemory_ReportsNullPointer) {
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NULL_POINTER, sizeStatus(nullptr, 0));
}

/**
 * Test that a size of zero is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, NoBytes_ReportsCorruptData) {
	Memory memory(1, 1);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		sizeStatus(memory.data(), 0));
}

/**
 * Test that memory far too short to hold a header is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, ThreeBytes_ReportsCorruptData) {
	Memory memory(3, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that memory one byte too short to hold a header is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, OneByteShort_ReportsCorruptData) {
	Memory memory(sizeof(Header) - 1, 1);
	setVersion(memory);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a negative size, which puts the end of the memory before its
 * start, is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, NegativeSize_ReportsCorruptData) {
	Memory memory(1, 1);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		sizeStatus(memory.data(), -1));
}

/**
 * Test that memory holding garbage is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, Garbage_ReportsIncorrectVersion) {
	Memory memory = garbage(64 * 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION, sizeStatus(memory));
}

/**
 * Test that a shift the header cannot declare is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, ShiftAboveLimit_ReportsIncorrectVersion) {
	Memory memory = emptyDataSet();
	setInt32(memory, offsetof(Header, profilesOffsetShift), 9);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION, sizeStatus(memory));
}

/**
 * Test that garbage after the version the engine reads is reported.
 */
TEST_F(IpiSizeManagerFromMemoryTests, GarbageWithVersion_ReportsCorruptData) {
	Memory memory = garbage(64 * 1024);
	setVersion(memory);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a collection that does not start where the one before it ended is
 * reported, after collections have been allocated.
 */
TEST_F(IpiSizeManagerFromMemoryTests, CollectionOutOfStep_ReportsCorruptData) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, maps), sizeof(Header) + 1, 0);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a collection longer than the memory is reported, after collections
 * have been allocated.
 */
TEST_F(IpiSizeManagerFromMemoryTests, CollectionTooLong_ReportsCorruptData) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, values), sizeof(Header), 1024);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that bytes left over after the last collection are reported, after all
 * the collections have been allocated.
 */
TEST_F(IpiSizeManagerFromMemoryTests, BytesLeftOver_ReportsOutOfBounds) {
	Memory memory = emptyDataSet();
	memory.resize(memory.size() + 16);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_POINTER_OUT_OF_BOUNDS,
		sizeStatus(memory));
}

/**
 * Test that a graph running past the end of the memory is reported. Only the
 * exception shows this failure, as the init method returns success.
 */
TEST_F(IpiSizeManagerFromMemoryTests, GraphTooLong_ReportsCorruptData) {
	Memory memory = graphTooLong();
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, sizeStatus(memory));
}

/**
 * Test that a failure only the exception shows is left in the exception. The
 * init method returns success for this one.
 */
TEST_F(IpiSizeManagerFromMemoryTests, ComponentMissing_ReportsOutOfRange) {
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
TEST_F(IpiSizeManagerFromMemoryTests, NoProperties_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		sizeStatus(memory));
}

/**
 * Test that a null configuration, which the comments on the method allow, gets
 * as far as reporting the status.
 */
TEST_F(IpiSizeManagerFromMemoryTests, NullConfig_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	TimeLimit limit(SizeMethodLimitSeconds);
	fillStackWithZeroedHandles();
	fiftyoneDegreesIpiSizeManagerFromMemory(
		nullptr,
		&properties,
		memory.data(),
		(fiftyoneDegreesFileOffset)memory.size(),
		exception);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT, exception->status);
}

/**
 * Test that every configuration reports the status, including the one with a
 * temporary file, which freeing a data set tries to delete.
 */
TEST_F(IpiSizeManagerFromMemoryTests, EveryConfig_ReportsNotPresent) {
	Memory memory = emptyDataSet();
	const fiftyoneDegreesConfigIpi * const configs[] = {
		&fiftyoneDegreesIpiInMemoryConfig,
		&fiftyoneDegreesIpiHighPerformanceConfig,
		&fiftyoneDegreesIpiLowMemoryConfig,
		&fiftyoneDegreesIpiBalancedConfig,
		&fiftyoneDegreesIpiBalancedTempConfig };
	for (const fiftyoneDegreesConfigIpi * const config : configs) {
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
			sizeStatus(
				memory.data(),
				(fiftyoneDegreesFileOffset)memory.size(),
				*config));
	}
}

/**
 * Test that a caller who passes no exception gets through a failure, and that
 * the allocation methods are back to the standard ones afterwards.
 */
TEST_F(IpiSizeManagerFromMemoryTests, NoException_RestoresAllocators) {
	Memory memory = garbage(64 * 1024);
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	{
		TimeLimit limit(SizeMethodLimitSeconds);
		fillStackWithZeroedHandles();
		fiftyoneDegreesIpiSizeManagerFromMemory(
			&config,
			&properties,
			memory.data(),
			(fiftyoneDegreesFileOffset)memory.size(),
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
TEST_F(IpiSizeManagerFromMemoryTests, FreeData_SizeLeavesMemoryWithCaller) {
	Memory memory = emptyDataSet();
	const Memory before = memory;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
	config.b.freeData = true;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		sizeStatus(
			memory.data(),
			(fiftyoneDegreesFileOffset)memory.size(),
			config));
	EXPECT_EQ(before, memory);
}

/**
 * Test that the init method leaves the memory with the caller on failure when
 * the configuration says to free it. The tracking free method fails on memory
 * it did not allocate, so freeing the memory here does not go unseen.
 */
TEST_F(IpiSizeManagerFromMemoryTests, FreeData_InitLeavesMemoryWithCaller) {
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
TEST_F(IpiSizeManagerFromMemoryTests, Init_CollectionFault_FreesAllocated) {
	Memory memory = emptyDataSet();
	setCollection(memory, offsetof(Header, maps), sizeof(Header) + 1, 0);
	size_t allocated;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		initTracked(memory, false, &allocated));
	EXPECT_EQ((size_t)0, allocated);
}

/**
 * Test that the init method frees what it allocated before a graph that runs
 * past the end of the memory, a failure only the exception shows.
 */
TEST_F(IpiSizeManagerFromMemoryTests, Init_GraphTooLong_FreesAllocated) {
	Memory memory = graphTooLong();
	size_t allocated;
	initTracked(memory, false, &allocated);
	EXPECT_EQ((size_t)0, allocated);
}

/**
 * Test that the init method frees the collections, graphs, lists and properties
 * allocated before finding there are no properties.
 */
TEST_F(IpiSizeManagerFromMemoryTests, Init_NoProperties_FreesAllocated) {
	Memory memory = emptyDataSet();
	size_t allocated;
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_REQ_PROP_NOT_PRESENT,
		initTracked(memory, false, &allocated));
	EXPECT_EQ((size_t)0, allocated);
}
