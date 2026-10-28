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

#include <gtest/gtest.h>
#include "TestUtils_Pointers.hpp"
#include "../collectionKeyTypes.h"
#include "../fiftyone.h"
#include <vector>

#ifdef FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT

namespace {

constexpr uint32_t recordCount = 16;

// Length of the record at index i, chosen so records are not a whole number
// of units and the alignment padding is exercised.
static uint32_t recordLength(const uint32_t i) {
	return 3 + (i % 8);
}

// A variable length collection whose records are aligned to 1 << shift byte
// boundaries relative to the start of the collection, and whose stored
// offsets and header length are counted in those units, as a version 4.6
// profiles collection is written.
class ShiftedCollection {
public:
	std::vector<byte> bytes;
	std::vector<uint32_t> offsets;
	fiftyoneDegreesCollection *collection = nullptr;

	ShiftedCollection(const byte shift, const uint32_t count) {
		const size_t unit = (size_t)1 << shift;
		for (uint32_t i = 0; i < recordCount; i++) {
			offsets.push_back((uint32_t)(bytes.size() / unit));
			for (uint32_t j = 0, n = recordLength(i); j < n; j++) {
				bytes.push_back((byte)(i + j));
			}
			while (bytes.size() % unit != 0) {
				bytes.push_back(0);
			}
		}
		fiftyoneDegreesMemoryReader reader = {
			bytes.data(),
			bytes.data(),
			bytes.data() + bytes.size(),
			(FileOffset)bytes.size(),
		};
		const fiftyoneDegreesCollectionHeader header = {
			0,
			(uint32_t)(bytes.size() / unit),
			count,
		};
		collection = fiftyoneDegreesCollectionCreateFromMemoryWithOffsetShift(
			&reader,
			header,
			shift);
	}

	~ShiftedCollection() {
		if (collection != nullptr) {
			collection->freeCollection(collection);
		}
	}
};

}

// A shifted collection must be variable length, so a header declaring a count
// is rejected rather than silently read as a fixed width collection with an
// element size derived from a length that is not in bytes.
TEST(CollectionOffsetShift, RejectsCountWithShift) {
	ShiftedCollection c(3, recordCount);
	EXPECT_EQ(nullptr, c.collection);
}

// A shift the 64 bit conversion cannot represent is rejected rather than
// invoking undefined behaviour on every access. An empty collection is used
// because for any other length the conversion overflows the reader first, so
// only this case reaches creation.
TEST(CollectionOffsetShift, RejectsShiftBeyondConversion) {
	byte data[8] = { 0 };
	fiftyoneDegreesMemoryReader reader = {
		data,
		data,
		data + sizeof(data),
		(FileOffset)sizeof(data),
	};
	const fiftyoneDegreesCollectionHeader header = { 0, 0, 0 };
	EXPECT_EQ(nullptr, fiftyoneDegreesCollectionCreateFromMemoryWithOffsetShift(
		&reader,
		header,
		33));
}

// A shift of zero is the existing behaviour and stays available to fixed
// width collections.
TEST(CollectionOffsetShift, AllowsCountWithoutShift) {
	ShiftedCollection c(0, recordCount);
	ASSERT_NE(nullptr, c.collection);
}

// Every record is returned at the byte position its unit offset denotes.
TEST(CollectionOffsetShift, ShiftedRecordsReadBack) {
	EXCEPTION_CREATE;
	ShiftedCollection c(3, 0);
	ASSERT_NE(nullptr, c.collection);
	for (uint32_t i = 0; i < recordCount; i++) {
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesCollectionKey key = {
			c.offsets[i],
			CollectionKeyType_Byte,
		};
		const byte * const ptr = (const byte*)c.collection->get(
			c.collection,
			&key,
			&item,
			exception);
		ASSERT_NE(nullptr, ptr);
		ASSERT_TRUE(EXCEPTION_OKAY);
		for (uint32_t j = 0, n = recordLength(i); j < n; j++) {
			EXPECT_EQ((byte)(i + j), ptr[j]);
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(c.collection, &item);
	}
}

#endif
