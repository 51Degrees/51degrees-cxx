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
 * @file MetaDataTextTests.cpp
 * @brief Checks the texts the metadata gives for properties, the rules for a
 * description, category or URL that is absent, the names of the
 * maps a property is in, a property found by name and the number of values
 * a property has.
 */

#include <string>
#include <vector>
#include "../../src/common/fiftyone.h"
#include "Base.hpp"
#include "StringCollection.hpp"
#include "FixedSizeCollection.hpp"

using std::string;
using std::vector;

/** A weighted value's URL offset, being the mask with a weight of half. */
static const int32_t metaDataTextWeighted = (int32_t)0xFF008000u;

class MetaDataTextTests : public Base {
public:
	void SetUp() override;
	void TearDown() override;

protected:
	/**
	 * A text of the property at the index, written with a buffer of the
	 * length given, or "<null>" when there is none.
	 */
	string propertyText(
		uint32_t index,
		fiftyoneDegreesPropertyText text,
		size_t length);

	/** The property at the index. */
	const fiftyoneDegreesProperty* property(uint32_t index);

	static constexpr int N_STRINGS = 12;
	static const char *strings[N_STRINGS];

	vector<fiftyoneDegreesProperty> propertyItems;
	StringCollection *stringsHelper = nullptr;
	FixedSizeCollection<fiftyoneDegreesProperty> *propertiesHelper = nullptr;
	FixedSizeCollection<int32_t> *mapsHelper = nullptr;
	FixedSizeCollection<fiftyoneDegreesValue> *valuesHelper = nullptr;
	fiftyoneDegreesCollection *stringsCollection = nullptr;
	fiftyoneDegreesCollection *properties = nullptr;
	fiftyoneDegreesCollection *maps = nullptr;
	fiftyoneDegreesCollection *values = nullptr;
	fiftyoneDegreesCollectionItem item;
};

const char *MetaDataTextTests::strings[] = {
	"first string in the file",
	"Name0",
	"Description0",
	"Category0",
	"https://51degrees.com/property0",
	"Lite",
	"Enterprise",
	"Name1",
	"ValueName",
	"ValueDescription",
	"https://51degrees.com/value",
	"Name3",
};

void MetaDataTextTests::SetUp() {
	stringsHelper = new StringCollection(strings, N_STRINGS);
	stringsCollection = stringsHelper->getState()->collection;
	const uint32_t *offsets = stringsHelper->getState()->offsets;

	// A property with every text, one with none, one named as the first,
	// and two whose value indexes are not both in the values collection.
	vector<fiftyoneDegreesProperty> &items = propertyItems;
	items.push_back({
		0, 0, 0, 0, 0, 0, 0,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
		0, // defaultValueIndex
		offsets[1], offsets[2], offsets[3], offsets[4],
		0, 2, // first and last value index
		2, 0 // map count and first map index
	});
	items.push_back({
		0, 0, 0, 0, 0, 0, 0,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
		UINT32_MAX, // no default value index
		offsets[7], UINT32_MAX, UINT32_MAX, 0,
		UINT32_MAX, UINT32_MAX,
		0, 0
	});
	items.push_back({
		0, 0, 0, 0, 0, 0, 0,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
		UINT32_MAX,
		offsets[1], UINT32_MAX, UINT32_MAX, 0,
		1, 5,
		0, 0
	});
	items.push_back({
		0, 0, 0, 0, 0, 0, 0,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
		UINT32_MAX,
		offsets[11], UINT32_MAX, UINT32_MAX, 0,
		2, 1,
		0, 0
	});
	propertiesHelper =
		new FixedSizeCollection<fiftyoneDegreesProperty>(items);
	properties = propertiesHelper->getState()->collection;

	mapsHelper = new FixedSizeCollection<int32_t>(
		{ (int32_t)offsets[5], (int32_t)offsets[6] });
	maps = mapsHelper->getState()->collection;

	// A value with a description and a URL, one with neither, and a weighted
	// one, whose URL offset holds its weight.
	vector<fiftyoneDegreesValue> valueItems;
	valueItems.push_back({
		0,
		(int32_t)offsets[8],
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
		(int32_t)offsets[9],
#endif
		(int32_t)offsets[10] });
	valueItems.push_back({
		0,
		(int32_t)offsets[8],
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
		0,
#endif
		0 });
	valueItems.push_back({
		0,
		(int32_t)offsets[8],
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
		-1,
#endif
		metaDataTextWeighted });
	valuesHelper = new FixedSizeCollection<fiftyoneDegreesValue>(valueItems);
	values = valuesHelper->getState()->collection;
	Base::SetUp();
}

void MetaDataTextTests::TearDown() {
	Base::TearDown();
	delete valuesHelper;
	delete mapsHelper;
	delete propertiesHelper;
	delete stringsHelper;
}

const fiftyoneDegreesProperty* MetaDataTextTests::property(uint32_t index) {
	return &propertyItems[index];
}

string MetaDataTextTests::propertyText(
	uint32_t index,
	fiftyoneDegreesPropertyText text,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesPropertyGetText(
		values,
		stringsCollection,
		nullptr,
		property(index),
		text,
		buffer.data(),
		buffer.size(),
		exception);
	if (result == nullptr) {
		return "<null>";
	}
	EXPECT_TRUE(EXCEPTION_OKAY);
	EXPECT_EQ(
		strlen(result) + 1 <= length,
		result == buffer.data());
	const string copy(result);
	if (result != buffer.data()) {
		fiftyoneDegreesFree(result);
	}
	return copy;
}

/**
 * A property's texts are its strings, written in place when they fit and
 * otherwise into memory the caller frees.
 */
TEST_F(MetaDataTextTests, PropertyTexts) {
	for (size_t length : { (size_t)64, (size_t)2 }) {
		EXPECT_EQ("Name0", propertyText(
			0, FIFTYONE_DEGREES_PROPERTY_TEXT_NAME, length));
		EXPECT_EQ("Description0", propertyText(
			0, FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION, length));
		EXPECT_EQ("Category0", propertyText(
			0, FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY, length));
		EXPECT_EQ("https://51degrees.com/property0", propertyText(
			0, FIFTYONE_DEGREES_PROPERTY_TEXT_URL, length));
		// The default is the name of the value at the default value index.
		EXPECT_EQ("ValueName", propertyText(
			0, FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE, length));
	}
}

/**
 * A description or category at offset -1 and a URL at offset 0 are absent,
 * so their texts are empty rather than the first string in the file, as is
 * the default of a property with no default value index.
 */
TEST_F(MetaDataTextTests, PropertyTextsAbsent) {
	for (size_t length : { (size_t)64, (size_t)1, (size_t)0 }) {
		EXPECT_EQ("Name1", propertyText(
			1, FIFTYONE_DEGREES_PROPERTY_TEXT_NAME, length));
		EXPECT_EQ("", propertyText(
			1, FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION, length));
		EXPECT_EQ("", propertyText(
			1, FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY, length));
		EXPECT_EQ("", propertyText(
			1, FIFTYONE_DEGREES_PROPERTY_TEXT_URL, length));
		EXPECT_EQ("", propertyText(
			1, FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE, length));
	}
}

/** A text that is none of those a property has gives nothing. */
TEST_F(MetaDataTextTests, PropertyTextNotKnown) {
	EXCEPTION_CREATE;
	char buffer[16];
	EXPECT_EQ(nullptr, fiftyoneDegreesPropertyGetText(
		values,
		stringsCollection,
		nullptr,
		property(0),
		(fiftyoneDegreesPropertyText)99,
		buffer,
		sizeof(buffer),
		exception));
	EXPECT_TRUE(EXCEPTION_CHECK(FIFTYONE_DEGREES_STATUS_INVALID_INPUT));
}

/**
 * The names of the maps a property is in, and nothing past the last one.
 */
TEST_F(MetaDataTextTests, PropertyMaps) {
	const char *expected[] = { "Lite", "Enterprise" };
	for (uint32_t m = 0; m < 2; m++) {
		for (size_t length : { (size_t)64, (size_t)2 }) {
			EXCEPTION_CREATE;
			vector<char> buffer(length);
			char *name = fiftyoneDegreesPropertyGetMapText(
				stringsCollection,
				maps,
				property(0),
				m,
				buffer.data(),
				buffer.size(),
				exception);
			ASSERT_NE(nullptr, name);
			EXPECT_TRUE(EXCEPTION_OKAY);
			EXPECT_STREQ(expected[m], name);
			if (name != buffer.data()) {
				fiftyoneDegreesFree(name);
			}
		}
	}
	EXCEPTION_CREATE;
	char buffer[16];
	EXPECT_EQ(nullptr, fiftyoneDegreesPropertyGetMapText(
		stringsCollection,
		maps,
		property(0),
		2,
		buffer,
		sizeof(buffer),
		exception));
	EXPECT_TRUE(EXCEPTION_CHECK(
		FIFTYONE_DEGREES_STATUS_COLLECTION_INDEX_OUT_OF_RANGE));
}

/**
 * A property is found by its exact name, the first with the name when two
 * share it, which is the one fiftyoneDegreesPropertyGetByName gets.
 */
TEST_F(MetaDataTextTests, PropertyIndexByName) {
	EXCEPTION_CREATE;
	EXPECT_EQ(0, fiftyoneDegreesPropertyGetIndexByName(
		properties, stringsCollection, "Name0", exception));
	EXPECT_EQ(1, fiftyoneDegreesPropertyGetIndexByName(
		properties, stringsCollection, "Name1", exception));
	EXPECT_EQ(3, fiftyoneDegreesPropertyGetIndexByName(
		properties, stringsCollection, "Name3", exception));
	EXPECT_EQ(-1, fiftyoneDegreesPropertyGetIndexByName(
		properties, stringsCollection, "name0", exception));
	EXPECT_EQ(-1, fiftyoneDegreesPropertyGetIndexByName(
		properties, stringsCollection, "Name", exception));
	EXPECT_TRUE(EXCEPTION_OKAY);
	const fiftyoneDegreesProperty *byName = fiftyoneDegreesPropertyGetByName(
		properties, stringsCollection, "Name0", &item, exception);
	ASSERT_NE(nullptr, byName);
	EXPECT_EQ(property(0)->firstValueIndex, byName->firstValueIndex);
	EXPECT_EQ(property(0)->lastValueIndex, byName->lastValueIndex);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(properties, &item);
}

/**
 * A property has the values from its first to its last value index, and
 * none when either is not in the values collection or the first is after
 * the last.
 */
TEST_F(MetaDataTextTests, PropertyValueCount) {
	EXPECT_EQ(3u, fiftyoneDegreesPropertyGetValueCount(values, property(0)));
	EXPECT_EQ(0u, fiftyoneDegreesPropertyGetValueCount(values, property(1)));
	EXPECT_EQ(0u, fiftyoneDegreesPropertyGetValueCount(values, property(2)));
	EXPECT_EQ(0u, fiftyoneDegreesPropertyGetValueCount(values, property(3)));
}
