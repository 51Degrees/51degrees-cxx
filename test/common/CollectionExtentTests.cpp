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
 * @file CollectionExtentTests.cpp
 * @brief Tests that a collection the file or the memory is too short to hold
 * is not created, however the collection is configured, and that a header is
 * not read past the end of the memory.
 */

#include "pch.h"
#include <cstring>
#include <vector>
#include "Base.hpp"
#include "FileHandle.hpp"
#include "../../src/common/collection.h"

/** The bytes the collection in each test declares, being four values. */
static const uint32_t declaredBytes = 16;

/** Each way a collection can be configured, in memory, on file, and cached. */
static const fiftyoneDegreesCollectionConfig configs[] = {
	{ true, 0, 0 },
	{ false, 0, 0 },
	{ false, 2, 1 },
};

class CollectionExtentTests : public Base {
protected:
	/**
	 * Gets the bytes of a collection, being the length it declares followed
	 * by only the number of bytes given.
	 */
	static std::vector<unsigned char> collectionBytes(const uint32_t present) {
		std::vector<unsigned char> bytes(sizeof(uint32_t) + present, 1);
		memcpy(bytes.data(), &declaredBytes, sizeof(uint32_t));
		return bytes;
	}

	/**
	 * True if a collection is created from a file holding the bytes, with
	 * the configuration given.
	 */
	static bool isCreatedFromFile(
		std::vector<unsigned char> bytes,
		const fiftyoneDegreesCollectionConfig &config) {
		FileHandle file(
			"collection-extent-test",
			bytes.data(),
			bytes.size(),
			1);
		fseek(file.getFile(), 0, SEEK_SET);
		fiftyoneDegreesCollection *collection =
			fiftyoneDegreesCollectionCreateFromFile(
				file.getFile(),
				file.getFilePool(),
				&config,
				fiftyoneDegreesCollectionHeaderFromFile(
					file.getFile(),
					sizeof(uint32_t),
					false),
				fiftyoneDegreesCollectionReadFileFixed);
		if (collection == nullptr) {
			return false;
		}
		collection->freeCollection(collection);
		return true;
	}
};

/**
 * Test that a collection the file holds all of is created in each
 * configuration.
 */
TEST_F(CollectionExtentTests, FromFile_Whole_IsCreated) {
	for (const fiftyoneDegreesCollectionConfig &config : configs) {
		EXPECT_TRUE(isCreatedFromFile(collectionBytes(declaredBytes), config))
			<< "Loaded " << config.loaded << ", capacity " << config.capacity;
	}
}

/**
 * Test that a collection the file is too short to hold is not created in
 * any configuration, half way and one byte short of its end.
 */
TEST_F(CollectionExtentTests, FromFile_Short_IsNotCreated) {
	for (const fiftyoneDegreesCollectionConfig &config : configs) {
		for (const uint32_t present : { declaredBytes / 2, declaredBytes - 1 }) {
			EXPECT_FALSE(isCreatedFromFile(collectionBytes(present), config))
				<< "Loaded " << config.loaded << ", capacity " <<
				config.capacity << ", " << present << " bytes present";
		}
	}
}

/**
 * Test that a collection the memory is too short to hold is not created.
 */
TEST_F(CollectionExtentTests, FromMemory_Short_IsNotCreated) {
	std::vector<unsigned char> bytes = collectionBytes(declaredBytes - 1);
	fiftyoneDegreesMemoryReader reader;
	reader.startByte = reader.current = bytes.data();
	reader.length = (fiftyoneDegreesFileOffset)bytes.size();
	reader.lastByte = reader.startByte + reader.length;
	fiftyoneDegreesCollection *collection =
		fiftyoneDegreesCollectionCreateFromMemory(
			&reader,
			fiftyoneDegreesCollectionHeaderFromMemory(
				&reader,
				sizeof(uint32_t),
				false));
	EXPECT_EQ(nullptr, collection);
	if (collection != nullptr) {
		collection->freeCollection(collection);
	}
}

/**
 * Test that a header the memory has too few bytes left for is read as no
 * collection, without reading the bytes beyond the end or moving past it.
 */
TEST_F(CollectionExtentTests, HeaderFromMemory_PastTheEnd_IsEmpty) {
	// The bytes after the end are set, so a read past it would show.
	unsigned char bytes[2 * sizeof(uint32_t)];
	memset(bytes, 0xFF, sizeof(bytes));
	fiftyoneDegreesMemoryReader reader;
	reader.startByte = reader.current = bytes;
	reader.length = (fiftyoneDegreesFileOffset)(sizeof(uint32_t) - 1);
	reader.lastByte = reader.startByte + reader.length;
	const fiftyoneDegreesCollectionHeader header =
		fiftyoneDegreesCollectionHeaderFromMemory(
			&reader,
			sizeof(uint32_t),
			false);
	EXPECT_EQ((uint32_t)0, header.length);
	EXPECT_EQ((uint32_t)0, header.count);
	EXPECT_EQ((fiftyoneDegreesFileOffsetUnsigned)0, header.startPosition);
	EXPECT_EQ(reader.startByte, reader.current);
}
