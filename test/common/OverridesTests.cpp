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
#include "../overrides.h"
#include "../memory.h"
#include "../string.h"
#include "StringCollection.hpp"

#ifdef _MSC_VER
// This is a mock implementation of the method
#pragma warning (disable: 4100)
#endif
static void overrideProfileId(void *state, uint32_t profileId) {
	(*(int*)state)++;
}
#ifdef _MSC_VER
#pragma warning (default: 4100)
#endif
// This test whether the ProfileIds evidence key is treated
// as case insensitive.
// Since the test only targets the case insensitivity aspect
// it does not check how the process ids string is processed,
// but whether the evidence is picked up.
TEST(OverrideProfileIdsTests, CaseSensitivity) {
	fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
		fiftyoneDegreesEvidenceCreate(1);

	// Test against upper cases
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		"51D_PROFILEIDS",
		"11-22-33-44");
	int state = 0;
	fiftyoneDegreesOverrideProfileIdMethod override
		= overrideProfileId;
	fiftyoneDegreesOverrideProfileIds(evidence, &state, override);
	EXPECT_EQ(4, state) <<
		"Case insentivity should be honoured for ProfileIds with upper case.";

	// Test against lower cases
	evidence->count = 0;
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		"51d_profileids",
		"55-66-77-88-99");

	state = 0;
	fiftyoneDegreesOverrideProfileIds(evidence, &state, override);
	EXPECT_EQ(5, state) <<
		"Case insentivity should be honoured for ProfileIds with lower case.";
    
    fiftyoneDegreesEvidenceFree(evidence);
}

// Check if overrides are set correctly
TEST(OverrideValuesResetTests, Positive) {
	uint32_t i, capacity = 10;
	fiftyoneDegreesOverrideValueArray *overrides = 
		fiftyoneDegreesOverrideValuesCreate(capacity);

	for (i = 0; i < capacity; i++) {
		fiftyoneDegreesOverridesAdd(overrides, i, "TestValue");
		EXPECT_EQ(i + 1, overrides->count);
		EXPECT_EQ(
			strlen("TestValue") + sizeof(fiftyoneDegreesString),
			overrides->items[i].string.allocated);
		EXPECT_STREQ(
			"TestValue",
			FIFTYONE_DEGREES_STRING(overrides->items[i].string.ptr));
	}

	fiftyoneDegreesOverrideValuesReset(overrides);
	EXPECT_EQ(0, overrides->count) << "Failed to reset overrides count.";
	for (i = 0; i < capacity; i++) {
		EXPECT_EQ(0, overrides->items[i].string.used);
		EXPECT_EQ(0, overrides->items[i].requiredPropertyIndex);
		EXPECT_STREQ("", FIFTYONE_DEGREES_STRING(overrides->items[i].string.ptr));
	}

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// Check if test doest not crash if reset a null pointer.
TEST(OverrideValuesResetTests, Negative) {
	fiftyoneDegreesOverrideValuesReset(NULL);
}

// The value that takes the last free item is stored, so the add reports
// success. Before the array reported whether room was left rather than
// whether the value was stored, which told the evidence walk to stop one
// item early.
TEST(OverridesAddTests, LastFreeItemIsStored) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(2);

	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 0, "first"));
	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 1, "second")) <<
		"The value that takes the last free item is stored.";
	EXPECT_EQ(2, overrides->count);
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		overrides->status) << "Filling the array is not a shortfall.";

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// A property the array does not hold yet needs a free item, so when the
// array is full it is not stored, the values already held are untouched, and
// the status says why.
TEST(OverridesAddTests, NewPropertyWhenFullIsRefused) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(1);

	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 0, "first"));
	EXPECT_FALSE(fiftyoneDegreesOverridesAdd(overrides, 1, "second")) <<
		"A new property cannot be stored in a full array.";
	EXPECT_EQ(1, overrides->count);
	EXPECT_STREQ(
		"first",
		FIFTYONE_DEGREES_STRING(overrides->items[0].string.ptr)) <<
		"The value already held should not change.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY,
		overrides->status) << "The caller should be able to read the "
		"shortfall.";

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// Replacing the value of a property the array already holds uses the item
// that property already has, so it works when the array is full. This is the
// case that empties a JavaScript property whose value was also sent as
// evidence.
TEST(OverridesAddTests, ExistingPropertyWhenFullIsReplaced) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(1);

	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 3, "first"));
	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 3, "")) <<
		"A property already in a full array can still be replaced.";
	EXPECT_EQ(1, overrides->count);
	EXPECT_STREQ(
		"",
		FIFTYONE_DEGREES_STRING(overrides->items[0].string.ptr));
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		overrides->status) << "Replacing a value is not a shortfall.";

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// A field that is not a property to be overridden, and a null array, are not
// shortfalls and nothing is stored.
TEST(OverridesAddTests, NoPropertyIndexStoresNothing) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(1);

	EXPECT_FALSE(fiftyoneDegreesOverridesAdd(overrides, -1, "value"));
	EXPECT_EQ(0, overrides->count);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, overrides->status);
	EXPECT_FALSE(fiftyoneDegreesOverridesAdd(NULL, 0, "value"));

	fiftyoneDegreesOverrideValuesFree(overrides);
}

#ifdef _MSC_VER
// The size is not needed by this mock implementation.
#pragma warning (disable: 4100)
#endif
static void* FIFTYONE_DEGREES_CALL_CONV failingMalloc(size_t size) {
	return NULL;
}
#ifdef _MSC_VER
#pragma warning (default: 4100)
#endif

// Where the value cannot be copied the property is taken out of the array
// rather than left as an item with no value to return, the status says why,
// and the values already held are unharmed. The array is still usable
// afterwards, and holds each allocation once, so freeing it is safe.
TEST(OverridesAddTests, ValueThatCannotBeCopiedLeavesNoItem) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(2);
	ASSERT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 7, "first"));

	void *(FIFTYONE_DEGREES_CALL_CONV *standardMalloc)(size_t) =
		fiftyoneDegreesMalloc;
	fiftyoneDegreesMalloc = failingMalloc;
	bool added = fiftyoneDegreesOverridesAdd(overrides, 8, "second");
	fiftyoneDegreesMalloc = standardMalloc;

	EXPECT_FALSE(added) << "A value that cannot be copied is not stored.";
	EXPECT_EQ(1, overrides->count) <<
		"The item taken for the property is given back.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
		overrides->status);
	EXPECT_EQ(7, overrides->items[0].requiredPropertyIndex);
	EXPECT_STREQ(
		"first",
		FIFTYONE_DEGREES_STRING(overrides->items[0].string.ptr)) <<
		"The value already held is unharmed.";

	EXPECT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 8, "third")) <<
		"The array is usable once the memory is there again.";
	EXPECT_STREQ(
		"third",
		FIFTYONE_DEGREES_STRING(overrides->items[1].string.ptr));

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// The same where the value cannot be copied over one the array already
// holds. The property is taken out, because the value it held has gone, and
// the other properties keep their values.
TEST(OverridesAddTests, ReplacementThatCannotBeCopiedLeavesNoItem) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(2);
	ASSERT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 7, "first"));
	ASSERT_TRUE(fiftyoneDegreesOverridesAdd(overrides, 8, "second"));

	void *(FIFTYONE_DEGREES_CALL_CONV *standardMalloc)(size_t) =
		fiftyoneDegreesMalloc;
	fiftyoneDegreesMalloc = failingMalloc;
	// Longer than the value held, so the item has to allocate rather than
	// write into the memory it already has.
	bool added = fiftyoneDegreesOverridesAdd(
		overrides,
		7,
		"a value longer than the one the item already holds");
	fiftyoneDegreesMalloc = standardMalloc;

	EXPECT_FALSE(added);
	EXPECT_EQ(1, overrides->count) <<
		"The property whose value could not be replaced is taken out.";
	EXPECT_EQ(8, overrides->items[0].requiredPropertyIndex) <<
		"The item left in use is the one for the other property.";
	EXPECT_STREQ(
		"second",
		FIFTYONE_DEGREES_STRING(overrides->items[0].string.ptr));
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
		overrides->status);

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// A reset returns the status to success so that a caller which reuses its
// results between requests reads a status for the request in hand.
TEST(OverrideValuesResetTests, StatusIsReset) {
	fiftyoneDegreesOverrideValueArray *overrides =
		fiftyoneDegreesOverrideValuesCreate(1);

	fiftyoneDegreesOverridesAdd(overrides, 0, "first");
	fiftyoneDegreesOverridesAdd(overrides, 1, "second");
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY,
		overrides->status);

	fiftyoneDegreesOverrideValuesReset(overrides);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, overrides->status);

	fiftyoneDegreesOverrideValuesFree(overrides);
}

// Property names used to build a properties structure for the extraction
// tests below.
static const char* overrideTestPropertyNames[] = {
	"Red",
	"Green",
	"Blue"
};

#ifdef _MSC_VER
// These are mock implementations of the methods.
#pragma warning (disable: 4100)
#endif
static uint32_t noEvidenceProperties(
	void *state,
	fiftyoneDegreesPropertyAvailable *property,
	fiftyoneDegreesEvidenceProperties *evidenceProperties) {
	return 0;
}

static bool everyPropertyOverridable(
	void *state,
	uint32_t requiredPropertyIndex) {
	return true;
}
#ifdef _MSC_VER
#pragma warning (default: 4100)
#endif

// Every cookie and query item is read even after the array has filled, so a
// value sent later in the evidence still replaces the value held for its
// property, and the values that could not be stored are reported. Before
// this the first value that filled the array ended the walk, and everything
// after it went unread.
TEST(OverridesExtractTests, EveryEvidenceItemIsReadWhenTheArrayFills) {
	StringCollection strings(overrideTestPropertyNames, 3);
	fiftyoneDegreesPropertiesRequired required;
	required.string = "Red,Green,Blue";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;

	fiftyoneDegreesPropertiesAvailable *available =
		fiftyoneDegreesPropertiesCreate(
			&required,
			strings.getState(),
			getStringValue,
			noEvidenceProperties);
	ASSERT_TRUE(available != NULL);

	fiftyoneDegreesOverridePropertyArray *properties =
		fiftyoneDegreesOverridePropertiesCreate(
			available,
			true,
			NULL,
			everyPropertyOverridable);
	ASSERT_TRUE(properties != NULL);

	// Room for one property only, which is what a caller that sizes the
	// array for something other than the evidence can end up with.
	fiftyoneDegreesOverrideValueArray *values =
		fiftyoneDegreesOverrideValuesCreate(1);

	fiftyoneDegreesEvidenceKeyValuePairArray *evidence =
		fiftyoneDegreesEvidenceCreate(3);
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "51D_Red", "one");
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "51D_Green", "two");
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_COOKIE, "51D_Red", "three");

	uint32_t read = fiftyoneDegreesOverridesExtractFromEvidence(
		properties,
		values,
		evidence);

	EXPECT_EQ(3, read) << "Every cookie and query item should be read.";
	EXPECT_EQ(1, values->count);
	EXPECT_STREQ(
		"three",
		FIFTYONE_DEGREES_STRING(values->items[0].string.ptr)) <<
		"The item read last should replace the value held for its property.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY,
		values->status) << "The value that did not fit should be reported.";

	fiftyoneDegreesEvidenceFree(evidence);
	fiftyoneDegreesOverrideValuesFree(values);
	fiftyoneDegreesOverridePropertiesFree(properties);
	fiftyoneDegreesPropertiesFree(available);
}

// With room for every property the evidence names, every value is stored and
// the status stays at success.
TEST(OverridesExtractTests, EveryValueIsStoredWhenThereIsRoom) {
	StringCollection strings(overrideTestPropertyNames, 3);
	fiftyoneDegreesPropertiesRequired required;
	required.string = "Red,Green,Blue";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;

	fiftyoneDegreesPropertiesAvailable *available =
		fiftyoneDegreesPropertiesCreate(
			&required,
			strings.getState(),
			getStringValue,
			noEvidenceProperties);
	ASSERT_TRUE(available != NULL);

	fiftyoneDegreesOverridePropertyArray *properties =
		fiftyoneDegreesOverridePropertiesCreate(
			available,
			true,
			NULL,
			everyPropertyOverridable);
	ASSERT_TRUE(properties != NULL);

	fiftyoneDegreesOverrideValueArray *values =
		fiftyoneDegreesOverrideValuesCreate(3);

	fiftyoneDegreesEvidenceKeyValuePairArray *evidence =
		fiftyoneDegreesEvidenceCreate(3);
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "51D_Red", "one");
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "51D_Green", "two");
	fiftyoneDegreesEvidenceAddString(
		evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "51D_Blue", "three");

	fiftyoneDegreesOverridesExtractFromEvidence(
		properties,
		values,
		evidence);

	EXPECT_EQ(3, values->count);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, values->status);

	fiftyoneDegreesEvidenceFree(evidence);
	fiftyoneDegreesOverrideValuesFree(values);
	fiftyoneDegreesOverridePropertiesFree(properties);
	fiftyoneDegreesPropertiesFree(available);
}
