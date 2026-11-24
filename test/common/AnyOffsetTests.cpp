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

#include "pch.h"
#include "../../src/common/fiftyone.h"
#include "../../src/common/collectionKeyTypes.h"
#include "StringCollection.hpp"
#include "ShiftedCollection.hpp"
#include <cstddef>
#include <cstring>
#include <vector>

// The records below are read straight from the data file, so their size and
// member offsets must match the file, and an alignment of 1 lets one start
// at any byte.
static_assert(alignof(fiftyoneDegreesValue) == 1,
	"Value must be packed at 1 byte");
static_assert(offsetof(fiftyoneDegreesValue, propertyIndex) == 0,
	"Value propertyIndex offset");
static_assert(offsetof(fiftyoneDegreesValue, nameOffset) == 2,
	"Value nameOffset offset");
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
static_assert(offsetof(fiftyoneDegreesValue, descriptionOffset) == 6,
	"Value descriptionOffset offset");
static_assert(offsetof(fiftyoneDegreesValue, urlOffsetOrWeight) == 10,
	"Value urlOffsetOrWeight offset");
static_assert(sizeof(fiftyoneDegreesValue) == 14, "Value size");
#else
static_assert(offsetof(fiftyoneDegreesValue, urlOffsetOrWeight) == 6,
	"Value urlOffsetOrWeight offset");
static_assert(sizeof(fiftyoneDegreesValue) == 10, "Value size");
#endif

static_assert(alignof(fiftyoneDegreesComponentKeyValuePair) == 1,
	"ComponentKeyValuePair must be packed at 1 byte");
static_assert(offsetof(fiftyoneDegreesComponentKeyValuePair, key) == 0,
	"ComponentKeyValuePair key offset");
static_assert(offsetof(fiftyoneDegreesComponentKeyValuePair, value) == 4,
	"ComponentKeyValuePair value offset");
static_assert(sizeof(fiftyoneDegreesComponentKeyValuePair) == 8,
	"ComponentKeyValuePair size");

static_assert(alignof(fiftyoneDegreesComponent) == 1,
	"Component must be packed at 1 byte");
static_assert(offsetof(fiftyoneDegreesComponent, keyValuesCount) == 9,
	"Component keyValuesCount offset");
static_assert(offsetof(fiftyoneDegreesComponent, firstKeyValuePair) == 11,
	"Component firstKeyValuePair offset");
static_assert(sizeof(fiftyoneDegreesComponent) == 19, "Component size");

static_assert(alignof(fiftyoneDegreesProfileOffset) == 1,
	"ProfileOffset must be packed at 1 byte");
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
static_assert(offsetof(fiftyoneDegreesProfileOffset, profileId) == 0,
	"ProfileOffset profileId offset");
static_assert(offsetof(fiftyoneDegreesProfileOffset, offset) == 4,
	"ProfileOffset offset offset");
static_assert(sizeof(fiftyoneDegreesProfileOffset) == 8,
	"ProfileOffset size");
#else
static_assert(offsetof(fiftyoneDegreesProfileOffset, offset) == 0,
	"ProfileOffset offset offset");
static_assert(sizeof(fiftyoneDegreesProfileOffset) == 4,
	"ProfileOffset size");
#endif

// Appends the bytes of a value to a record buffer.
template <typename T>
static void append(std::vector<byte> &bytes, const T &value) {
	const size_t end = bytes.size();
	bytes.resize(end + sizeof(T));
	memcpy(bytes.data() + end, &value, sizeof(T));
}

/**
 * The count or length header of a collection, and the 32 bit integers in it,
 * are read at every byte offset.
 */
TEST(AnyOffset, CollectionHeaderAndIntegers) {
	const int32_t integers[] = { 1, -2, 0x12345678, INT32_MIN };
	const uint32_t count = sizeof(integers) / sizeof(integers[0]);
	std::vector<byte> records;
	for (int32_t integer : integers) {
		append(records, integer);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		ShiftedCollection shifted(records, sizeof(int32_t), shift);
		EXPECT_EQ((uint32_t)records.size(), shifted.header.length);
		EXPECT_EQ(count, shifted.header.count);
		for (uint32_t i = 0; i < count; i++) {
			EXPECT_EQ(integers[i], fiftyoneDegreesCollectionGetInteger32(
				shifted.collection,
				i,
				exception));
			EXPECT_TRUE(EXCEPTION_OKAY);
		}

		// The same header read as a count of items.
		fiftyoneDegreesMemoryReader reader;
		reader.startByte = shifted.start;
		reader.current = shifted.start;
		reader.length = (fiftyoneDegreesFileOffset)sizeof(uint32_t);
		reader.lastByte = shifted.start + reader.length;
		const fiftyoneDegreesCollectionHeader countHeader =
			fiftyoneDegreesCollectionHeaderFromMemory(&reader, 2, true);
		EXPECT_EQ((uint32_t)records.size(), countHeader.count);
		EXPECT_EQ((uint32_t)records.size() * 2, countHeader.length);
	}
}

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY
/**
 * A string or byte array read from a file starts with its 16 bit length,
 * which the file reader passes in a byte buffer with no alignment.
 */
TEST(AnyOffset, LengthPrefixes) {
	const uint16_t length = 0x0123;
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		alignas(8) byte buffer[MAX_SHIFT_BYTES + 1 + sizeof(length)] = { 0 };
		memcpy(buffer + shift, &length, sizeof(length));
		EXPECT_EQ(sizeof(length) + length,
			fiftyoneDegreesStringGetFinalSize(buffer + shift, exception));
		EXPECT_EQ(sizeof(length) + length,
			CollectionKeyType_IPAddress->getFinalSizeMethod(
				buffer + shift,
				exception));
	}
}
#endif

/**
 * A collection of plain 32 bit profile offsets, as IP Intelligence has, read
 * through the function its profile iteration uses.
 */
TEST(AnyOffset, ProfileOffsetsAsPureOffsets) {
	const uint32_t offsets[] = { 0, 9, 0x01020304, UINT32_MAX };
	std::vector<byte> records;
	for (uint32_t offset : offsets) {
		append(records, offset);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		ShiftedCollection shifted(records, sizeof(uint32_t), shift);
		for (uint32_t i = 0; i < shifted.header.count; i++) {
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			const fiftyoneDegreesCollectionKey key = {
				{ i },
				CollectionKeyType_ProfileOffset,
			};
			const void *raw = shifted.collection->get(
				shifted.collection,
				&key,
				&item,
				exception);
			ASSERT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, raw);
			EXPECT_EQ(offsets[i],
				fiftyoneDegreesProfileOffsetAsPureOffset(raw));
			FIFTYONE_DEGREES_COLLECTION_RELEASE(shifted.collection, &item);
		}
	}
}

#ifndef FIFTYONE_DEGREES_REDUCED_FILE

// Profiles with no values, so each is 9 bytes and the second and later start
// at odd offsets.
static std::vector<byte> createProfiles(
	const std::vector<uint32_t> &profileIds,
	std::vector<uint32_t> &offsets) {
	std::vector<byte> records;
	for (uint32_t profileId : profileIds) {
		const fiftyoneDegreesProfile profile = { 0, profileId, 0 };
		offsets.push_back((uint32_t)records.size());
		append(records, profile);
	}
	return records;
}

/**
 * Profiles found by id through a collection of plain 32 bit profile offsets
 * at every byte offset.
 */
TEST(AnyOffset, ProfileByIdThroughPlainOffsets) {
	const std::vector<uint32_t> profileIds = { 3, 17, 101, 4000 };
	std::vector<uint32_t> offsets;
	const std::vector<byte> profiles = createProfiles(profileIds, offsets);
	std::vector<byte> offsetRecords;
	for (uint32_t offset : offsets) {
		append(offsetRecords, offset);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		ShiftedCollection shiftedProfiles(profiles, 0, shift);
		ShiftedCollection shiftedOffsets(
			offsetRecords,
			sizeof(uint32_t),
			MAX_SHIFT_BYTES - shift);
		for (uint32_t profileId : profileIds) {
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			const fiftyoneDegreesProfile *profile =
				fiftyoneDegreesProfileGetByProfileIdIndirect(
					shiftedOffsets.collection,
					shiftedProfiles.collection,
					profileId,
					&item,
					exception);
			EXPECT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, profile) << profileId;
			const uint32_t foundId = profile->profileId;
			EXPECT_EQ(profileId, foundId);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(item.collection, &item);
		}
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		EXPECT_EQ(nullptr, fiftyoneDegreesProfileGetByProfileIdIndirect(
			shiftedOffsets.collection,
			shiftedProfiles.collection,
			50,
			&item,
			exception));
	}
}

/**
 * Profiles found by id through ProfileOffset records at every byte offset.
 */
TEST(AnyOffset, ProfileOffsetRecords) {
	const std::vector<uint32_t> profileIds = { 3, 17, 101, 4000 };
	std::vector<uint32_t> offsets;
	const std::vector<byte> profiles = createProfiles(profileIds, offsets);
	std::vector<byte> offsetRecords;
	for (size_t i = 0; i < profileIds.size(); i++) {
		const fiftyoneDegreesProfileOffset record = {
			profileIds[i],
			offsets[i]
		};
		append(offsetRecords, record);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		ShiftedCollection shiftedProfiles(profiles, 0, shift);
		ShiftedCollection shiftedOffsets(
			offsetRecords,
			sizeof(fiftyoneDegreesProfileOffset),
			MAX_SHIFT_BYTES - shift);
		for (uint32_t i = 0; i < profileIds.size(); i++) {
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			const fiftyoneDegreesProfile *profile =
				fiftyoneDegreesProfileGetByProfileId(
					shiftedOffsets.collection,
					shiftedProfiles.collection,
					profileIds[i],
					&item,
					exception);
			EXPECT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, profile) << profileIds[i];
			uint32_t foundId = profile->profileId;
			EXPECT_EQ(profileIds[i], foundId);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(item.collection, &item);

			profile = fiftyoneDegreesProfileGetByIndex(
				shiftedOffsets.collection,
				shiftedProfiles.collection,
				i,
				&item,
				exception);
			EXPECT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, profile) << i;
			foundId = profile->profileId;
			EXPECT_EQ(profileIds[i], foundId);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(item.collection, &item);
		}
	}
}

#endif

/**
 * Value records at every byte offset give the right name.
 */
TEST(AnyOffset, ValueRecords) {
	const char *names[] = { "First", "Second", "Third" };
	const int count = (int)(sizeof(names) / sizeof(names[0]));
	StringCollection strings(names, count);
	const uint32_t *nameOffsets = strings.getState()->offsets;
	std::vector<byte> records;
	for (int i = 0; i < count; i++) {
		const fiftyoneDegreesValue value = {
			(int16_t)i, // propertyIndex
			(int32_t)nameOffsets[i], // nameOffset
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
			(int32_t)nameOffsets[i], // descriptionOffset
#endif
			(int32_t)nameOffsets[i], // urlOffsetOrWeight
		};
		append(records, value);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		ShiftedCollection values(records, sizeof(fiftyoneDegreesValue), shift);
		for (int i = 0; i < count; i++) {
			fiftyoneDegreesCollectionItem valueItem, nameItem;
			fiftyoneDegreesDataReset(&valueItem.data);
			fiftyoneDegreesDataReset(&nameItem.data);
			const fiftyoneDegreesValue *value = fiftyoneDegreesValueGet(
				values.collection,
				(uint32_t)i,
				&valueItem,
				exception);
			ASSERT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, value);
			const fiftyoneDegreesString *name = fiftyoneDegreesValueGetName(
				strings.getState()->collection,
				value,
				&nameItem,
				exception);
			ASSERT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, name);
			EXPECT_STREQ(names[i], &name->value);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(nameItem.collection, &nameItem);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(
				valueItem.collection,
				&valueItem);
		}
	}
}

/**
 * A component and its key value pairs at every byte offset.
 */
TEST(AnyOffset, ComponentKeyValuePairs) {
	const uint16_t pairCount = 3;
	std::vector<byte> record;
	append(record, (byte)7); // componentId
	append(record, (int32_t)0); // nameOffset
	append(record, (int32_t)0); // defaultProfileOffset
	append(record, pairCount); // keyValuesCount
	for (uint32_t i = 0; i < pairCount; i++) {
		const fiftyoneDegreesComponentKeyValuePair pair = {
			0x100 + i,
			0x200 + i
		};
		append(record, pair);
	}
	for (size_t shift = 0; shift <= MAX_SHIFT_BYTES; shift++) {
		SCOPED_TRACE(shift);
		EXCEPTION_CREATE
		alignas(8) byte buffer[MAX_SHIFT_BYTES + 64] = { 0 };
		ASSERT_LE(shift + record.size(), sizeof(buffer));
		memcpy(buffer + shift, record.data(), record.size());
		fiftyoneDegreesComponent *component =
			(fiftyoneDegreesComponent*)(buffer + shift);
		EXPECT_EQ((uint32_t)record.size(),
			fiftyoneDegreesComponentGetFinalSize(component, exception));
		for (uint16_t i = 0; i < pairCount; i++) {
			const fiftyoneDegreesComponentKeyValuePair *pair =
				fiftyoneDegreesComponentGetKeyValuePair(
					component,
					i,
					exception);
			ASSERT_TRUE(EXCEPTION_OKAY);
			ASSERT_NE(nullptr, pair);
			const uint32_t key = pair->key;
			const uint32_t value = pair->value;
			EXPECT_EQ(0x100u + i, key);
			EXPECT_EQ(0x200u + i, value);
		}
#ifndef FIFTYONE_DEGREES_EXCEPTIONS_DISABLED
		// The index after the last pair is outside the component.
		EXPECT_EQ(nullptr, fiftyoneDegreesComponentGetKeyValuePair(
			component,
			pairCount,
			exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_CHECK(
			FIFTYONE_DEGREES_STATUS_COLLECTION_INDEX_OUT_OF_RANGE));
#endif
	}
}
