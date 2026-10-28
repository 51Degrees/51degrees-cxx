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

#include "../fiftyone.h"
#include "../indices.h"
#include "../profile.h"
#include "../value.h"

#include "Base.hpp"
#include "FixedSizeCollection.hpp"
#include "VariableSizeCollection.hpp"
#include <vector>

// The property profile index is only built for full size data files, where
// profiles carry an id.
#ifndef FIFTYONE_DEGREES_REDUCED_FILE

namespace {

constexpr uint32_t INDICES_PROPERTIES = 3;
constexpr uint32_t INDICES_VALUES_PER_PROPERTY = 2;

// A profile with room for a value for every property. The valueCount of the
// profile says how many of the slots are in use, so a profile can have fewer
// values than there are properties.
#pragma pack(push, 1)
typedef struct {
	fiftyoneDegreesProfile profile;
	uint32_t valueIndices[INDICES_PROPERTIES];
} IndicesProfileContainer;
#pragma pack(pop)

// Allocator used while the index is created. It fills every allocation with
// zero bytes so that any entry the index does not write reads as zero, which
// is what a fresh page from the operating system holds. Without it the
// unwritten entries hold whatever the heap last held, and a missing
// initialisation would only fail some of the time.
void* (FIFTYONE_DEGREES_CALL_CONV* savedMalloc)(size_t size) = NULL;

void* FIFTYONE_DEGREES_CALL_CONV zeroFilledMalloc(size_t size) {
	void* pointer = savedMalloc(size);
	if (pointer != NULL) {
		memset(pointer, 0, size);
	}
	return pointer;
}

bool collectIndicesValues(void* state, fiftyoneDegreesCollectionItem* item) {
	std::vector<const fiftyoneDegreesValue*>* found =
		(std::vector<const fiftyoneDegreesValue*>*)state;
	found->push_back((const fiftyoneDegreesValue*)item->data.ptr);
	return true;
}

} // namespace

/**
 * Builds a small data set where the index has entries that no profile
 * writes.
 *
 * Values 0 and 1 belong to property 0, values 2 and 3 to property 1, and
 * values 4 and 5 to property 2. The name offset of each value is set to its
 * own index so the tests can tell which value came back.
 *
 * Profile 10 has values 0 and 4, so it has no value for property 1.
 * Profile 11 does not exist, so the index has a gap for it.
 * Profile 12 has value 3 only, so it has no value for properties 0 and 2.
 * Profile 13 has values 1, 2 and 5, one for every property.
 *
 * The available properties are given in the order 2, 0, 1, so the index has
 * to sort them before it can match them to the values of a profile.
 */
class IndicesTests : public Base {
public:
	IndicesTests() {
		std::vector<fiftyoneDegreesValue> values;
		for (uint32_t i = 0;
			i < INDICES_PROPERTIES * INDICES_VALUES_PER_PROPERTY;
			i++) {
			values.push_back({
				(int16_t)(i / INDICES_VALUES_PER_PROPERTY), // propertyIndex
				(int32_t)i, // nameOffset, used here to identify the value
				(int32_t)i, // descriptionOffset
				(int32_t)i, // urlOffsetOrWeight
			});
		}
		valuesHelper =
			new FixedSizeCollection<fiftyoneDegreesValue>(values);
		valuesCollection = valuesHelper->getState()->collection;

		std::vector<fiftyoneDegreesProperty> properties;
		for (byte i = 0; i < INDICES_PROPERTIES; i++) {
			uint32_t first = i * INDICES_VALUES_PER_PROPERTY;
			properties.push_back({
				0, // componentIndex
				i, // displayOrder
				1, // isMandatory
				0, // isList
				1, // showValues
				0, // isObsolete
				1, // show
				FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, // valueType
				first, // defaultValueIndex
				0, // nameOffset
				0, // descriptionOffset
				0, // categoryOffset
				0, // urlOffset
				first, // firstValueIndex
				first + INDICES_VALUES_PER_PROPERTY - 1, // lastValueIndex
				0, // mapCount
				0 // firstMapIndex
			});
		}
		propertiesHelper =
			new FixedSizeCollection<fiftyoneDegreesProperty>(properties);
		propertiesCollection = propertiesHelper->getState()->collection;

		std::vector<IndicesProfileContainer> profiles;
		profiles.push_back({ { 0, 10, 2 }, { 0, 4, 0 } });
		profiles.push_back({ { 0, 12, 1 }, { 3, 0, 0 } });
		profiles.push_back({ { 0, 13, 3 }, { 1, 2, 5 } });
		profilesHelper =
			new VariableSizeCollection<IndicesProfileContainer>(profiles);
		profilesCollection = profilesHelper->getState()->collection;

		std::vector<fiftyoneDegreesProfileOffset> offsets;
		for (size_t i = 0; i < profiles.size(); i++) {
			offsets.push_back({
				profiles[i].profile.profileId,
				profilesHelper->getState()->offsets[i]
			});
		}
		profileOffsetsHelper =
			new FixedSizeCollection<fiftyoneDegreesProfileOffset>(offsets);
		profileOffsetsCollection =
			profileOffsetsHelper->getState()->collection;
	}

	virtual ~IndicesTests() {
		delete profileOffsetsHelper;
		delete profilesHelper;
		delete propertiesHelper;
		delete valuesHelper;
	}

	void SetUp() override {
		Base::SetUp();
		FIFTYONE_DEGREES_ARRAY_CREATE(
			fiftyoneDegreesPropertyAvailable,
			available,
			INDICES_PROPERTIES);
		ASSERT_NE(available, nullptr);
		for (uint32_t i = 0; i < INDICES_PROPERTIES; i++) {
			fiftyoneDegreesPropertyAvailable* item =
				&available->items[available->count++];
			item->propertyIndex = AVAILABLE_ORDER[i];
			fiftyoneDegreesDataReset(&item->name.data);
			item->name.collection = NULL;
			item->name.handle = NULL;
			item->evidenceProperties = NULL;
			item->delayExecution = false;
		}

		// Create the index with every allocation filled with zero bytes.
		FIFTYONE_DEGREES_EXCEPTION_CREATE
		savedMalloc = fiftyoneDegreesMalloc;
		fiftyoneDegreesMalloc = zeroFilledMalloc;
		index = fiftyoneDegreesIndicesPropertyProfileCreate(
			profilesCollection,
			profileOffsetsCollection,
			available,
			valuesCollection,
			exception);
		fiftyoneDegreesMalloc = savedMalloc;
		ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		ASSERT_NE(index, nullptr);
	}

	void TearDown() override {
		if (index != NULL) {
			fiftyoneDegreesIndicesPropertyProfileFree(index);
			index = NULL;
		}
		if (available != NULL) {
			fiftyoneDegreesFree(available);
			available = NULL;
		}
		Base::TearDown();
	}

	// Returns the position of the property in the available properties.
	uint32_t availableIndexOf(uint32_t propertyIndex) {
		for (uint32_t i = 0; i < INDICES_PROPERTIES; i++) {
			if (AVAILABLE_ORDER[i] == propertyIndex) {
				return i;
			}
		}
		return UINT32_MAX;
	}

	// Returns the name offsets, which are the value indexes, of the values
	// the index gives for the profile and property.
	std::vector<int32_t> valuesFor(
		uint32_t profileId,
		uint32_t propertyIndex) {
		std::vector<int32_t> result;
		std::vector<const fiftyoneDegreesValue*> found;
		fiftyoneDegreesCollectionItem profileItem;
		fiftyoneDegreesCollectionItem propertyItem;
		fiftyoneDegreesDataReset(&profileItem.data);
		fiftyoneDegreesDataReset(&propertyItem.data);
		FIFTYONE_DEGREES_EXCEPTION_CREATE
		fiftyoneDegreesProfile* profile =
			fiftyoneDegreesProfileGetByProfileId(
				profileOffsetsCollection,
				profilesCollection,
				profileId,
				&profileItem,
				exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		EXPECT_NE(profile, nullptr);
		const fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
			propertiesCollection,
			propertyIndex,
			&propertyItem,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		EXPECT_NE(property, nullptr);
		if (profile != NULL && property != NULL) {
			fiftyoneDegreesProfileIterateValuesForPropertyWithIndex(
				valuesCollection,
				index,
				availableIndexOf(propertyIndex),
				profile,
				property,
				&found,
				collectIndicesValues,
				exception);
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
			for (const fiftyoneDegreesValue* value : found) {
				result.push_back(value->nameOffset);
			}
		}
		if (property != NULL) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(
				propertiesCollection,
				&propertyItem);
		}
		if (profile != NULL) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(
				profilesCollection,
				&profileItem);
		}
		return result;
	}

	static constexpr uint32_t AVAILABLE_ORDER[INDICES_PROPERTIES] = {
		2, 0, 1 };

	FixedSizeCollection<fiftyoneDegreesValue>* valuesHelper;
	FixedSizeCollection<fiftyoneDegreesProperty>* propertiesHelper;
	VariableSizeCollection<IndicesProfileContainer>* profilesHelper;
	FixedSizeCollection<fiftyoneDegreesProfileOffset>* profileOffsetsHelper;
	fiftyoneDegreesCollection* valuesCollection;
	fiftyoneDegreesCollection* propertiesCollection;
	fiftyoneDegreesCollection* profilesCollection;
	fiftyoneDegreesCollection* profileOffsetsCollection;
	fiftyoneDegreesPropertiesAvailable* available = NULL;
	fiftyoneDegreesIndicesPropertyProfile* index = NULL;
};

constexpr uint32_t IndicesTests::AVAILABLE_ORDER[INDICES_PROPERTIES];

/**
 * A profile with no value for a property has no entry in the index.
 */
TEST_F(IndicesTests, MissingPropertyHasNoEntry) {
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(
			index, 10, availableIndexOf(1)));
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(
			index, 12, availableIndexOf(0)));
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(
			index, 12, availableIndexOf(2)));
}

/**
 * A profile id between the lowest and highest that is not in the data set
 * has no entries in the index.
 */
TEST_F(IndicesTests, ProfileIdGapHasNoEntry) {
	for (uint32_t i = 0; i < INDICES_PROPERTIES; i++) {
		EXPECT_EQ(
			FIFTYONE_DEGREES_INDICES_NO_VALUE,
			fiftyoneDegreesIndicesPropertyProfileLookup(index, 11, i))
			<< "available property " << i;
	}
}

/**
 * A profile id, or an available property, outside the range of the index is
 * refused rather than read from beyond the end of the index.
 */
TEST_F(IndicesTests, OutsideRangeHasNoEntry) {
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(index, 9, 0));
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(index, 14, 0));
	EXPECT_EQ(
		FIFTYONE_DEGREES_INDICES_NO_VALUE,
		fiftyoneDegreesIndicesPropertyProfileLookup(
			index, 10, INDICES_PROPERTIES));
}

/**
 * Asking for a property the profile has no value for returns nothing, and in
 * particular not the value of an earlier property of the same profile.
 */
TEST_F(IndicesTests, MissingPropertyReturnsNoValues) {
	EXPECT_TRUE(valuesFor(10, 1).empty());
	EXPECT_TRUE(valuesFor(12, 0).empty());
	EXPECT_TRUE(valuesFor(12, 2).empty());
}

/**
 * The values a profile does have are still returned, for every property and
 * with the available properties given out of order.
 */
TEST_F(IndicesTests, PresentPropertiesReturnTheirValues) {
	EXPECT_EQ(std::vector<int32_t>({ 0 }), valuesFor(10, 0));
	EXPECT_EQ(std::vector<int32_t>({ 4 }), valuesFor(10, 2));
	EXPECT_EQ(std::vector<int32_t>({ 3 }), valuesFor(12, 1));
	EXPECT_EQ(std::vector<int32_t>({ 1 }), valuesFor(13, 0));
	EXPECT_EQ(std::vector<int32_t>({ 2 }), valuesFor(13, 1));
	EXPECT_EQ(std::vector<int32_t>({ 5 }), valuesFor(13, 2));
}

/**
 * The reader refuses a starting position whose value belongs to an earlier
 * property, even if the index holds such a position. The entry for profile
 * 12 and property 2 is set to 0, which points at value 3 of property 1.
 * Value 3 is below the first value of property 2, so nothing is returned.
 */
TEST_F(IndicesTests, ReaderRefusesValueOfEarlierProperty) {
	uint32_t entry =
		(12 - index->minProfileId) * index->availablePropertyCount +
		availableIndexOf(2);
	ASSERT_LT(entry, index->size);
	index->valueIndexes[entry] = 0;
	EXPECT_TRUE(valuesFor(12, 2).empty());
}

#endif
