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

#include <string>
#include <vector>
#include "../../src/common/RequiredPropertiesConfig.hpp"

using namespace FiftyoneDegrees::Common;

vector<string> testVector = { "Red", "Green", "Blue" };

/* This contain the same properties as testVector */
string testString = "Red,Green,Blue";

/**
 * Check two string vectors are equal. This means they each contain all, and
 * only, the values of the other.
 */
void assertPropertiesAreEqual(vector<string> *actual, vector<string> *expected) {
	int i;
	ASSERT_EQ(actual->size(), expected->size()) <<
		L"The vectors are different sizes.";
	for (i = 0; i < (int)actual->size(); i++) {
		ASSERT_STREQ(
			actual->operator[](i).c_str(),
			expected->operator[](i).c_str()) <<
			L"String values are not equal.";
	}
}

/**
 * Check that the properties set in the constructor are returned correctly.
 */
TEST(RequiredPropertiesConfig, CreateWithString) {
	RequiredPropertiesConfig *conf =
		new RequiredPropertiesConfig(&testString);
	vector<string> properties = conf->getProperties();
	assertPropertiesAreEqual(&properties, &testVector);
	delete conf;
}

/**
 * Check that the properties set in the constructor are returned correctly.
 */
TEST(RequiredPropertiesConfig, CreateWithVector) {
	RequiredPropertiesConfig *conf =
		new RequiredPropertiesConfig(&testVector);
	vector<string> properties = conf->getProperties();
	assertPropertiesAreEqual(&properties, &testVector);
	delete conf;
}

/**
 * Check that the properties returned by the instance are copied of the argument
 * passed to the constructor, and not a pointer.
 */
TEST(RequiredPropertiesConfig, VectorCopied) {
	vector<string> propertiesVector = testVector;
	RequiredPropertiesConfig *conf = new RequiredPropertiesConfig(
		&propertiesVector);
	propertiesVector = { "non" };
	vector<string> properties = conf->getProperties();
	assertPropertiesAreEqual(&properties, &testVector);
	delete conf;
}
/**
 * Check that the properties returned by the instance are copied of the argument
 * passed to the constructor, and not a pointer.
 */
TEST(RequiredPropertiesConfig, StringCopied) {
	string propertiesString = testString;
	RequiredPropertiesConfig *conf = new RequiredPropertiesConfig(
		&propertiesString);
	propertiesString = "non";
	vector<string> properties = conf->getProperties();
	assertPropertiesAreEqual(&properties, &testVector);
	delete conf;
}

/**
 * Check that every property in a comma separated string is returned each time
 * getProperties is called, and that the count does not change.
 */
TEST(RequiredPropertiesConfig, StringGetPropertiesRepeated) {
	RequiredPropertiesConfig conf(&testString);
	int count = conf.getCount();
	for (int i = 0; i < 3; i++) {
		vector<string> properties = conf.getProperties();
		assertPropertiesAreEqual(&properties, &testVector);
		EXPECT_EQ(count, conf.getCount());
	}
}

/**
 * Check that containsProperty sees every property in a comma separated
 * string each time it is called, in any case, and in any order.
 */
TEST(RequiredPropertiesConfig, StringContainsPropertyRepeated) {
	RequiredPropertiesConfig conf(&testString);
	for (int i = 0; i < 3; i++) {
		for (auto it = testVector.rbegin(); it != testVector.rend(); ++it) {
			EXPECT_TRUE(conf.containsProperty(*it)) <<
				"Property '" << *it << "' was not found on pass " << i;
		}
		EXPECT_TRUE(conf.containsProperty("green"));
		EXPECT_TRUE(conf.containsProperty("BLUE"));
		EXPECT_FALSE(conf.containsProperty("Gree"));
		EXPECT_FALSE(conf.containsProperty("Greens"));
		EXPECT_FALSE(conf.containsProperty("Red,Green"));
		EXPECT_FALSE(conf.containsProperty(""));
	}
	vector<string> properties = conf.getProperties();
	assertPropertiesAreEqual(&properties, &testVector);
}

/**
 * Check that empty items in a comma separated string are skipped.
 */
TEST(RequiredPropertiesConfig, StringEmptyItemsSkipped) {
	RequiredPropertiesConfig conf(",Red,,Green,Blue,");
	for (int i = 0; i < 2; i++) {
		vector<string> properties = conf.getProperties();
		assertPropertiesAreEqual(&properties, &testVector);
	}
	EXPECT_FALSE(conf.containsProperty(""));
}