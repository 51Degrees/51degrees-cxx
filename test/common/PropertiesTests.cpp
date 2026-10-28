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
#include "Base.hpp"
#include "StringCollection.hpp"
#include "../properties.h"
#include "../collectionKeyTypes.h"

// Property names
static const char* testValues[] = {
	"Red",
	"Yellow",
	"Green",
	"Blue",
	"Brown",
	"Black",
	"White",
	"Yellowjs",
	"SetHeaderBrowserAccept-CH"
};

/**
 * Properties test class used to test the functionality of properties.c.
 */
class Properties : public Base {
protected:
	StringCollection *strings;
	int count;
	fiftyoneDegreesPropertiesAvailable *properties = nullptr;

	/**
	 * Calls the base setup method to enable memory leak checking and memory
	 * allocation checking. Also constructs a string collection to be used by
	 * the test methods.
	 */
	void SetUp() {
		Base::SetUp();
		count = sizeof(testValues) / sizeof(const char*);
		strings = new StringCollection(testValues, count);
	}

	/**
	 * Releases the properties structure if one was created, and the
	 * StringCollection instance. Then calls the base teardown method to check
	 * for memory leaks and compare expected and actual memory allocations.
	 */
	void TearDown() {
		if (properties != nullptr) {
			fiftyoneDegreesPropertiesFree(properties);
		}
		delete strings;
		Base::TearDown();
	}

	/**
	 * Create a properties structure from the specified required properties
	 * using the method in properties.c. The expected memory allocation is
	 * calculated, and the actual memory allocation is tracked. The structure
	 * is freed automatically after each test, at which point the expected and
	 * actual memory allocation is checked for equality.
	 * @param required the properties used to create the properties structure
	 */
	void CreateProperties(fiftyoneDegreesPropertiesRequired *required) {
		properties = fiftyoneDegreesPropertiesCreate(
			required,
			strings->getState(),
			getStringValue,
			getEvidenceProperties);
	}

	/**
	 * Create four required properties, Red, Yellow, Green and Blue, which
	 * sort to Blue, Green, Red, Yellow in the required property order.
	 */
	void CreateFourProperties() {
		fiftyoneDegreesPropertiesRequired required;
		required.string = "Red,Yellow,Green,Blue";
		required.array = NULL;
		required.count = 0;
		required.existing = NULL;
		CreateProperties(&required);
	}

	static uint32_t getEvidenceProperties(
		void* state,
		fiftyoneDegreesPropertyAvailable* property,
		fiftyoneDegreesEvidenceProperties* evidenceProperties) {
		int count = 0;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		stringCollectionState *stringState = (stringCollectionState*)state;
		fiftyoneDegreesString* name = (fiftyoneDegreesString*)property->name.data.ptr;
		char* jsName = (char*)malloc(name->size + (sizeof(char) * strlen("js")));
		strcpy(jsName, &name->value);
		strcpy(jsName + name->size - 1, "js");
		
		for (uint32_t i = 0; i < stringState->count; i++) {
			const fiftyoneDegreesCollectionKey key {
				stringState->offsets[i],
				CollectionKeyType_String,
			};
			auto const currentName = (const fiftyoneDegreesString*)
				stringState->collection->get(
					stringState->collection,
					&key,
					&item,
					exception);
			if (strcmp(&currentName->value, jsName) == 0) {
				if (evidenceProperties != NULL) {
					evidenceProperties->items[count] = i;
				}
				count++;
			}
		}
		free(jsName);

		// Only call the release macro if there is a release method. This check
		// is needed because GTest does not support configurations other than
		// DEBUG and RELEASE. Where 51Degrees uses MEMORY-ONLY and 
		// SINGLE-THREADED suffixes these do not work with GTest. Therefore
		// the macro might not reflect the implementation in the library that
		// implements the collection and items that will have been compiled
		// with the correct pre processor directive.
		if (stringState->collection->release != NULL) {
			FIFTYONE_DEGREES_COLLECTION_RELEASE(
				stringState->collection, 
				&item);
		}
		return count;
	}

};

/**
 * Check that all the properties are present as expected.
 */
TEST_F(Properties, AllProperties) {
	CreateProperties(NULL);
	for (int i = 0; i < count; i++) {
		int reqIndex =
			fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
				this->properties,
				testValues[i]);
		const char *name = FIFTYONE_DEGREES_STRING(
			fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
				this->properties,
				reqIndex));
		EXPECT_STREQ(testValues[i], name);
	}
}

/**
 * Check that passing a string list of required properties works as expected.
 * Yellow is in the list of properties and required properties so should 
 * have an index.
 * Red is in the list of properties but not required properties so should
 * return an index of -1.
 * Beige is not in the list of properties but is in required properties so 
 * should also return an index of -1.
 */
TEST_F(Properties, OneMissingProperty) {
	fiftyoneDegreesPropertiesRequired required;
	required.string = "Yellow,Beige";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
	int reqIndex = fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Yellow");
	EXPECT_EQ(0, reqIndex);
	const char *name = FIFTYONE_DEGREES_STRING(
		fiftyoneDegreesPropertiesGetNameFromRequiredIndex(
			this->properties,
			reqIndex));
	EXPECT_STREQ("Yellow", name);
	EXPECT_EQ(-1, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Beige"));
	EXPECT_EQ(-1,fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Red"));
}

/**
 * Check that passing a string list of required properties works as expected.
 * Yellow and Black are both in the list of properties and required properties 
 * so should have index values.
 * Since they are sorted alphabetically, Black should have index 0 and Yellow 
 * index 1.
 */
TEST_F(Properties, StringTwoPropertiesOrdered) {
	fiftyoneDegreesPropertiesRequired required;
	required.string = "Yellow,Black";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
	EXPECT_EQ(0, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Black"));
	EXPECT_EQ(1, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Yellow"));
}

/**
 * Check that passing a string list of required properties works as expected.
 * Function should still work for other properties if there is a property
 * corrupted by a space.
 */
TEST_F(Properties, StringTwoPropertiesOrderedSpace) {
	fiftyoneDegreesPropertiesRequired required;
	required.string = "Cyan,Yellow, Black ,Blue|";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
	EXPECT_EQ(0, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Black"));
	EXPECT_EQ(1, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Blue"));
	EXPECT_EQ(2, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Yellow"));
}

/**
 * Check that passing an array of strings of required properties works as expected.
 * Yellow and Black are both in the list of properties and required properties 
 * so should have index values.
 * Since they are sorted alphabetically, Black should have index 0 and Yellow 
 * index 1.
 */
TEST_F(Properties, ArrayTwoPropertiesOrdered) {
	const char* tests[] = { "Yellow", "Black" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	EXPECT_EQ(0, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Black"));
	EXPECT_EQ(1, fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
		this->properties,
		"Yellow"));
}

/**
 * Check that passing an empty string works as expected. This should not throw
 * an error and should behave in the same way as if the string was null.
 */
TEST_F(Properties, StringEmpty) {
	fiftyoneDegreesPropertiesRequired required;
	required.string = "";
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
}

/**
 * Check that adding two identical properties in an array only yields results
 * with a single instance of the property.
 */
TEST_F(Properties, RepeatedArray) {
	const char* tests[]{ "Yellow", "Yellow" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	ASSERT_EQ(1, this->properties->count);
}

/**
 * Check that adding two identical properties in a string only yields results
 * with a single instance of the property.
 */
TEST_F(Properties, RepeatedString) {
	const char* tests = "Yellow,Yellow";
	fiftyoneDegreesPropertiesRequired required;
	required.string = tests;
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
	ASSERT_EQ(1, this->properties->count);
}

/**
 * Check that adding two properties which are the same when case is ignored
 * only yields results with a single instance of the property.
 */
 TEST_F(Properties, RepeatedArray_DifferentCase) {
	const char* tests[]{ "yellow", "Yellow" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	ASSERT_EQ(1, this->properties->count);
}

/**
 * Check that adding two properties which are the same when case is ignored
 * only yields results with a single instance of the property.
 */
TEST_F(Properties, RepeatedString_DifferentCase) {
	const char* tests = "yellow,Yellow";
	fiftyoneDegreesPropertiesRequired required;
	required.string = tests;
	required.array = NULL;
	required.count = 0;
	required.existing = NULL;
	CreateProperties(&required);
	ASSERT_EQ(1, this->properties->count);
}

/**
 * Check that case is not taken into account when fetching the required
 * property index i.e. get('property') == get('PROPERTY').
 */
TEST_F(Properties, CaseInsensitiveGetIndex) {
	const char* tests[]{ "yellow" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	ASSERT_NE(
		-1,
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"yellow"));
	ASSERT_EQ(
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"Yellow"),
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"yellow"));
}

/**
 * Check that an evidence property for a required property
 * is correctly set in the property's evidence property array.
 */
TEST_F(Properties, EvidenceProperties) {
	const char* tests[]{ "yellow", "yellowjs" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	int yellowIndex =
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"yellow");
	int yellowJsIndex =
		fiftyoneDegreesPropertiesGetPropertyIndexFromName(
			this->properties,
			"yellowjs");
	ASSERT_NE(-1, yellowIndex);
	ASSERT_NE(-1, yellowJsIndex);

	ASSERT_EQ(
		1,
		this->properties->items[yellowIndex].evidenceProperties->count);
	ASSERT_EQ(
		yellowJsIndex,
		this->properties->items[yellowIndex].evidenceProperties->items[0]);
}

/**
 * Check that no evidence properties are added to a required property
 * which has no evidence properties.
 */
TEST_F(Properties, EvidenceProperties_None) {
	const char* tests[]{ "red", "yellowjs" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	int redIndex =
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"red");
	int yellowJsIndex =
		fiftyoneDegreesPropertiesGetPropertyIndexFromName(
			this->properties,
			"yellowjs");
	ASSERT_NE(-1, redIndex);
	ASSERT_NE(-1, yellowJsIndex);

	ASSERT_EQ(
		0,
		this->properties->items[redIndex].evidenceProperties->count);
}

/**
 * Check that an evidence property for a required property
 * is correctly set in the property's evidence property array when the
 * evidence property is not in the required properties.
 */
TEST_F(Properties, EvidenceProperties_NotRequired) {
	const char* tests[]{ "yellow" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = tests;
	required.count = sizeof(tests) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	int yellowIndex =
		fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
			this->properties,
			"yellow");
	int yellowJsIndex =
		fiftyoneDegreesPropertiesGetPropertyIndexFromName(
			this->properties,
			"yellowjs");
	ASSERT_NE(-1, yellowIndex);
	ASSERT_EQ(-1, yellowJsIndex);

	ASSERT_EQ(
		1,
		this->properties->items[yellowIndex].evidenceProperties->count);
	ASSERT_NE(
		-1,
		this->properties->items[yellowIndex].evidenceProperties->items[0]);
}

/**
 * Check that 'SetHeader' properties is included in the list of 
 * required properties.
 */
TEST_F(Properties, EvidenceProperties_IsSetHeaderRequired) {
	const char* testsIncluded[]{ "Blue", "SetHeaderBrowserAccept-CH" };
	const char* testsNotIncluded[]{ "Blue", "SetHeaderBrowserNotRequired" };
	fiftyoneDegreesPropertiesRequired required;
	required.string = NULL;
	required.array = testsIncluded;
	required.count = sizeof(testsIncluded) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	// Check if the 'SetHeader' property is included in the available
	// required properties
	bool isIncluded = fiftyoneDegreesPropertiesIsSetHeaderAvailable(properties);
	ASSERT_TRUE(isIncluded);
	if (properties != nullptr) {
		fiftyoneDegreesPropertiesFree(properties);
	}

	required.string = NULL;
	required.array = testsNotIncluded;
	required.count = sizeof(testsIncluded) / sizeof(const char*);
	required.existing = NULL;
	CreateProperties(&required);
	// Check if the 'SetHeader' property is not included in the available
	// required properties
	isIncluded = fiftyoneDegreesPropertiesIsSetHeaderAvailable(properties);
	ASSERT_FALSE(isIncluded);
}

/**
 * Component mask tests. The mask is built from required property indexes
 * using the componentIndex recorded on each available property, which the
 * fixture sets by hand because it has no data set to read them from.
 */
TEST_F(Properties, ComponentIndexDefaultsToZero) {
	CreateFourProperties();
	for (uint32_t i = 0; i < properties->count; i++) {
		EXPECT_EQ(0, properties->items[i].componentIndex);
	}
}

TEST_F(Properties, ComponentMaskNullOrNegativeEnablesEverything) {
	CreateFourProperties();
	int one[] = { 0 };
	EXPECT_EQ(FIFTYONE_DEGREES_COMPONENT_MASK_ALL,
		fiftyoneDegreesPropertiesGetComponentMask(properties, NULL, 0));
	EXPECT_EQ(FIFTYONE_DEGREES_COMPONENT_MASK_ALL,
		fiftyoneDegreesPropertiesGetComponentMask(properties, one, -1));
}

TEST_F(Properties, ComponentMaskEmptyEnablesNothing) {
	CreateFourProperties();
	int none[] = { 0 };
	EXPECT_EQ(0u,
		fiftyoneDegreesPropertiesGetComponentMask(properties, none, 0));
}

TEST_F(Properties, ComponentMaskSetsOneBitPerComponent) {
	CreateFourProperties();
	// Sorted order is Blue, Green, Red, Yellow. Give them components
	// 1, 3, 1 and 0 so two properties share a component.
	properties->items[0].componentIndex = 1;
	properties->items[1].componentIndex = 3;
	properties->items[2].componentIndex = 1;
	properties->items[3].componentIndex = 0;
	int blueAndGreen[] = { 0, 1 };
	EXPECT_EQ(0xAu,
		fiftyoneDegreesPropertiesGetComponentMask(properties, blueAndGreen, 2));
	int yellow[] = { 3 };
	EXPECT_EQ(0x1u,
		fiftyoneDegreesPropertiesGetComponentMask(properties, yellow, 1));
	int blueAndRed[] = { 0, 2 };
	EXPECT_EQ(0x2u,
		fiftyoneDegreesPropertiesGetComponentMask(properties, blueAndRed, 2)) <<
		"Two properties on one component set one bit.";
}

TEST_F(Properties, ComponentMaskIgnoresBadIndexes) {
	CreateFourProperties();
	properties->items[0].componentIndex = 2;
	int indexes[] = { -1, 0, (int)properties->count, 100000 };
	EXPECT_EQ(0x4u,
		fiftyoneDegreesPropertiesGetComponentMask(properties, indexes, 4));
}

TEST_F(Properties, ComponentMaskCannotAddressComponent32) {
	CreateFourProperties();
	properties->items[0].componentIndex = 32;
	int indexes[] = { 0 };
	EXPECT_EQ(0u,
		fiftyoneDegreesPropertiesGetComponentMask(properties, indexes, 1));
	// Beyond the mask a component is always enabled, so the caller still
	// gets a correct result for a data set with more than 32 components.
	// A runtime value, because a constant 32 would make the compiler warn about
	// the shift the short circuit never reaches.
	uint32_t beyond = properties->items[0].componentIndex;
	EXPECT_TRUE(FIFTYONE_DEGREES_COMPONENT_MASK_ENABLED(0u, beyond));
	EXPECT_FALSE(FIFTYONE_DEGREES_COMPONENT_MASK_ENABLED(0u, 0));
	EXPECT_TRUE(FIFTYONE_DEGREES_COMPONENT_MASK_ENABLED(1u << 5, 5));
	EXPECT_FALSE(FIFTYONE_DEGREES_COMPONENT_MASK_ENABLED(1u << 5, 4));
}
