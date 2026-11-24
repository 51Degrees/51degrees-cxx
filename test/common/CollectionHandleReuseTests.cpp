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
 * @file CollectionHandleReuseTests.cpp
 * @brief Reads a variable size collection and a fixed size collection that
 * follows it in the same file, with one file handle, so that each read uses
 * the handle the read before it used. Every item must be read as it is in
 * the file, whatever was read with the handle before.
 */

#include "pch.h"
#include <vector>
#include "Base.hpp"
#include "FileHandle.hpp"
#include "../../src/common/collection.h"

// Items in the variable size collection. Each is a count of one, then its
// own index, so an item read from the wrong place has the wrong index.
static const uint32_t reuseVariableItems = 2048;
static const uint32_t reuseVariableItemSize = 2 * sizeof(uint32_t);

// Items in the fixed size collection that follows it.
static const uint32_t reuseFixedItems = 16;

static uint32_t getReuseItemSize(
	const void *initial,
	fiftyoneDegreesException * const exception) {
	(void)exception;
	return (uint32_t)(sizeof(uint32_t) * ((*(uint32_t*)initial) + 1));
}

static const fiftyoneDegreesCollectionKeyType reuseVariableKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(uint32_t),
	getReuseItemSize,
};

static const fiftyoneDegreesCollectionKeyType reuseFixedKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(uint32_t),
	NULL,
};

static void* reuseVariableRead(
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

class CollectionHandleReuseTests : public Base {
protected:
	FileHandle *fileHandle = nullptr;
	fiftyoneDegreesCollection *variable = nullptr;
	fiftyoneDegreesCollection *fixed = nullptr;

	void SetUp() override {
		Base::SetUp();

		// The variable size collection, with its length first, then the
		// fixed size collection, with its count first.
		std::vector<uint32_t> data;
		data.push_back(reuseVariableItems * reuseVariableItemSize);
		for (uint32_t i = 0; i < reuseVariableItems; i++) {
			data.push_back(1);
			data.push_back(i);
		}
		data.push_back(reuseFixedItems);
		for (uint32_t i = 0; i < reuseFixedItems; i++) {
			data.push_back(0xF0000000 + i);
		}

		// One handle, so that every read uses it.
		fileHandle = new FileHandle(
			"collection-handle-reuse-test",
			data.data(),
			data.size() * sizeof(uint32_t),
			1);
		// The helper has already failed the test with the file name.
		ASSERT_TRUE(fileHandle->isOpen());
		fiftyoneDegreesCollectionConfig config = { false, 0, 1 };
		FILE *file = fileHandle->getFile();
		fseek(file, 0, SEEK_SET);
		variable = fiftyoneDegreesCollectionCreateFromFile(
			file,
			fileHandle->getFilePool(),
			&config,
			fiftyoneDegreesCollectionHeaderFromFile(file, 0, false),
			reuseVariableRead);
		fseek(
			file,
			(long)(sizeof(uint32_t) +
				reuseVariableItems * reuseVariableItemSize),
			SEEK_SET);
		fixed = fiftyoneDegreesCollectionCreateFromFile(
			file,
			fileHandle->getFilePool(),
			&config,
			fiftyoneDegreesCollectionHeaderFromFile(
				file,
				sizeof(uint32_t),
				true),
			fiftyoneDegreesCollectionReadFileFixed);
	}

	void TearDown() override {
		if (fixed != nullptr) {
			fixed->freeCollection(fixed);
		}
		if (variable != nullptr) {
			variable->freeCollection(variable);
		}
		delete fileHandle;
		Base::TearDown();
	}

	// Reads the variable size item and checks it holds its own index.
	void checkVariable(uint32_t index) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesCollectionKey key = {
			{ index * reuseVariableItemSize },
			&reuseVariableKeyType,
		};
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const uint32_t *value = (const uint32_t*)variable->get(
			variable,
			&key,
			&item,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
			"Variable item " << index << " could not be read.";
		ASSERT_NE(nullptr, value) <<
			"Variable item " << index << " could not be read.";
		EXPECT_EQ(1U, value[0]) <<
			"Variable item " << index << " was read from the wrong place.";
		EXPECT_EQ(index, value[1]) <<
			"Variable item " << index << " was read from the wrong place.";
		FIFTYONE_DEGREES_COLLECTION_RELEASE(variable, &item);
	}

	// Reads the fixed size item and checks its value.
	void checkFixed(uint32_t index) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesCollectionKey key = {
			{ index },
			&reuseFixedKeyType,
		};
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const uint32_t *value = (const uint32_t*)fixed->get(
			fixed,
			&key,
			&item,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		ASSERT_NE(nullptr, value);
		EXPECT_EQ(0xF0000000 + index, *value);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(fixed, &item);
	}
};

/**
 * Reads a variable size item from the start of the file, which leaves the
 * rest of the stream's buffer holding the bytes after it. Then reads the
 * first fixed size item, which follows the variable size items, and then
 * variable size items just before it. Each must hold its own index rather
 * than bytes left in the buffer by the first read.
 */
TEST_F(CollectionHandleReuseTests, VariableAfterFixed_ReadsTheFile) {
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		return;
	}
	ASSERT_NE(nullptr, variable);
	ASSERT_NE(nullptr, fixed);
	const uint32_t beforeFixed[] = {
		reuseVariableItems - 1,
		reuseVariableItems - 16,
		reuseVariableItems - 128,
		reuseVariableItems - 256 };
	for (uint32_t index : beforeFixed) {
		checkVariable(0);
		checkFixed(0);
		checkVariable(index);
	}
}

/**
 * Reads every variable size item, each after a fixed size item, and every
 * fixed size item after a variable size item.
 */
TEST_F(CollectionHandleReuseTests, Alternate_ReadsTheFile) {
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		return;
	}
	ASSERT_NE(nullptr, variable);
	ASSERT_NE(nullptr, fixed);
	for (uint32_t i = 0; i < reuseVariableItems; i++) {
		checkFixed(i % reuseFixedItems);
		checkVariable(i);
		checkVariable((i * 7919) % reuseVariableItems);
	}
}
