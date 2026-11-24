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
 * @file CollectionFailedReadTests.cpp
 * @brief Reads a variable size item whose header is in the file but whose
 * body is cut short. The read must report the failure and leave the data
 * with nothing allocated, so that the item can be released and its memory
 * used again.
 */

#include "pch.h"
#include "Base.hpp"
#include "FileHandle.hpp"
#include "../../src/common/collection.h"

// Each item is a count followed by that many integers. The last item's count
// says it holds more integers than the file has after it, so its header can
// be read but its body cannot.
static const uint32_t failedReadValues[] = {
	2, 10, 11,
	1, 20,
	100, 30, 31 };

// Offsets of the items within the collection.
static const uint32_t failedReadWholeOffset = 0;
static const uint32_t failedReadCutOffset = 5 * sizeof(uint32_t);

static uint32_t getFailedReadItemSize(
	const void *initial,
	fiftyoneDegreesException * const exception) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(exception);
#	endif
	return (uint32_t)(sizeof(uint32_t) * ((*(uint32_t*)initial) + 1));
}

static const fiftyoneDegreesCollectionKeyType failedReadKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(uint32_t),
	getFailedReadItemSize,
};

static void* failedReadItemRead(
	const fiftyoneDegreesCollectionFile * const file,
	const fiftyoneDegreesCollectionKey * const key,
	fiftyoneDegreesData * const data,
	fiftyoneDegreesException * const exception) {
	uint32_t count;
	return fiftyoneDegreesCollectionReadFileVariable(
		file,
		data,
		key,
		&count,
		exception);
}

class CollectionFailedReadTests : public Base {
protected:
	FileHandle *fileHandle = nullptr;
	fiftyoneDegreesCollection *collection = nullptr;
	byte data[sizeof(uint32_t) + sizeof(failedReadValues)];

	void SetUp() override {
		Base::SetUp();
		*(uint32_t*)data = (uint32_t)sizeof(failedReadValues);
		memcpy(
			data + sizeof(uint32_t),
			failedReadValues,
			sizeof(failedReadValues));
		fileHandle = new FileHandle(
			"collection-failed-read-test",
			data,
			sizeof(data),
			1);
	}

	void TearDown() override {
		if (collection != nullptr) {
			collection->freeCollection(collection);
		}
		delete fileHandle;
		Base::TearDown();
	}

	void create(fiftyoneDegreesCollectionConfig config) {
		fseek(fileHandle->getFile(), 0, SEEK_SET);
		collection = fiftyoneDegreesCollectionCreateFromFile(
			fileHandle->getFile(),
			fileHandle->getFilePool(),
			&config,
			fiftyoneDegreesCollectionHeaderFromFile(
				fileHandle->getFile(),
				0,
				false),
			failedReadItemRead);
		ASSERT_NE(nullptr, collection) <<
			"The collection was not created.";
	}

	// Gets the item at the offset, checks the read reported the failure and
	// left the item holding nothing, then releases it.
	void getCutItem() {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesCollectionKey key = {
			{ failedReadCutOffset },
			&failedReadKeyType,
		};
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		EXPECT_EQ(nullptr, collection->get(collection, &key, &item, exception))
			<< "An item whose body could not be read was returned.";
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED);
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_COLLECTION_FILE_READ_FAIL,
			exception->status);
		EXPECT_EQ(nullptr, item.data.ptr);
		EXPECT_EQ(0U, item.data.allocated);
		EXPECT_EQ(0U, item.data.used);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(collection, &item);
	}

	// Gets the whole item, which must still be read correctly after a read
	// that failed.
	void getWholeItem() {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesCollectionKey key = {
			{ failedReadWholeOffset },
			&failedReadKeyType,
		};
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const uint32_t *value = (const uint32_t*)collection->get(
			collection,
			&key,
			&item,
			exception);
		ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		ASSERT_NE(nullptr, value);
		EXPECT_EQ(2U, value[0]);
		EXPECT_EQ(10U, value[1]);
		EXPECT_EQ(11U, value[2]);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(collection, &item);
	}
};

/**
 * Reads the cut item straight from the file into data that the caller
 * provides. After the failure the data must hold nothing, as the memory the
 * read allocated has been freed.
 */
TEST_F(CollectionFailedReadTests, File_ReadVariable_DataHoldsNothing) {
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		return;
	}
	create({ false, 0, 1 });
	fiftyoneDegreesCollectionFile *file =
		(fiftyoneDegreesCollectionFile*)collection->state;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const fiftyoneDegreesCollectionKey key = {
		{ failedReadCutOffset },
		&failedReadKeyType,
	};
	uint32_t count;
	fiftyoneDegreesData itemData;
	fiftyoneDegreesDataReset(&itemData);
	EXPECT_EQ(nullptr, fiftyoneDegreesCollectionReadFileVariable(
		file,
		&itemData,
		&key,
		&count,
		exception));
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_COLLECTION_FILE_READ_FAIL,
		exception->status);
	EXPECT_EQ(nullptr, itemData.ptr) <<
		"The data still points to the memory the failed read freed.";
	EXPECT_EQ(0U, itemData.allocated) <<
		"The data still counts the memory the failed read freed.";
	EXPECT_EQ(0U, itemData.used);

	// Data that holds nothing can be used for the next read.
	if (itemData.allocated == 0) {
		FIFTYONE_DEGREES_EXCEPTION_CLEAR;
		const fiftyoneDegreesCollectionKey wholeKey = {
			{ failedReadWholeOffset },
			&failedReadKeyType,
		};
		const uint32_t *value = (const uint32_t*)
			fiftyoneDegreesCollectionReadFileVariable(
				file,
				&itemData,
				&wholeKey,
				&count,
				exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		ASSERT_NE(nullptr, value);
		EXPECT_EQ(2U, value[0]);
		EXPECT_EQ(11U, value[2]);
		fiftyoneDegreesFree(itemData.ptr);
	}
}

/**
 * Gets the cut item from a collection read from the file for each get, as
 * the LowMemory configuration does, and releases it.
 */
TEST_F(CollectionFailedReadTests, File_GetAndRelease) {
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		return;
	}
	create({ false, 0, 1 });
	getCutItem();
	getWholeItem();
	getCutItem();
}

/**
 * Gets the cut item through a cache, whose loader reads from the file, and
 * releases it. The node the load failed on is used again for the next load.
 */
TEST_F(CollectionFailedReadTests, Cache_GetAndRelease) {
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		return;
	}
	create({ false, 2, 1 });
	getCutItem();
	getWholeItem();
	getCutItem();
}
