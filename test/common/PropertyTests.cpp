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

#include "../../src/common/fiftyone.h"
#include "../../src/common/string.h"
#include "Base.hpp"
#include "limits.h"
#include "StringCollection.hpp"
#include "FixedSizeCollection.hpp"

class PropertyTests : public Base {
public:
    virtual void SetUp();
    virtual void TearDown();
    void CreateObjects();
    void assessProperty(const fiftyoneDegreesProperty *property, int i);
    
    fiftyoneDegreesProperty *property;
    StringCollection *stringsCollectionHelper;
    fiftyoneDegreesCollection *stringsCollection;
    
    fiftyoneDegreesCollectionItem item;
    
    FixedSizeCollection<fiftyoneDegreesProperty> *propertiesCollectionHelper;
    fiftyoneDegreesCollection *propertiesCollection;
    
    static constexpr int N_PROPERTIES = 3;
    static constexpr int N_PER_PROPERTY = 5;
    static const char *strings[N_PROPERTIES * N_PER_PROPERTY];
    static const fiftyoneDegreesPropertyValueType types[N_PROPERTIES];

};

const fiftyoneDegreesPropertyValueType PropertyTests::types[N_PROPERTIES] = {
   FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_JAVASCRIPT,
   FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
   FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DOUBLE,
};

const char *PropertyTests::strings[] = {
    "prop1", "name", "descr", "cat", "url",
    "prop2", "name2", "descr2", "cat2", "url2",
    "prop3", "name3", "descr3", "cat3", "url3"
};

void PropertyTests::CreateObjects() {
    stringsCollectionHelper = new StringCollection(strings, sizeof(strings)/sizeof(strings[0]));
    stringsCollection = stringsCollectionHelper->getState()->collection;
    uint32_t *offsets = stringsCollectionHelper->getState()->offsets;
    std::vector<fiftyoneDegreesProperty> tempProperties;
    for (byte i=0;i<N_PROPERTIES;++i) {
        int propIdx = i * N_PER_PROPERTY;
        tempProperties.push_back({
            0, // componentIndex
            i, // displayOrder
            1, // isMandatory
            0, // isList
            1, // showValues
            0, // isObsolete
            1, // show
            (byte) types[i], // valueType
            20, // defaultValueIndex;
            offsets[propIdx + 1], // nameOffset
            offsets[propIdx + 2], // descriptionOffset
            offsets[propIdx + 3], // categoryOffset
            offsets[propIdx + 4], // urlOffset
            5, // firstValueIndex
            15, // lastValueIndex
            4, // mapCount
            0 // firstMapIndex
        });
    }
    propertiesCollectionHelper = new FixedSizeCollection<fiftyoneDegreesProperty>(tempProperties);
    propertiesCollection = propertiesCollectionHelper->getState()->collection;
}

void PropertyTests::SetUp() {
    CreateObjects();
    Base::SetUp();
}

void PropertyTests::TearDown() {
    Base::TearDown();
    delete stringsCollectionHelper;
    delete propertiesCollectionHelper;
}

void PropertyTests::assessProperty(const fiftyoneDegreesProperty *p, int i) {
    EXCEPTION_CREATE
    
    const String *name = fiftyoneDegreesPropertyGetName(stringsCollection, p, &item, exception);
    EXPECT_STREQ(strings[i * N_PER_PROPERTY + 1], &name->value);
    
    const String *descr = fiftyoneDegreesPropertyGetDescription(stringsCollection, p, &item, exception);
    EXPECT_STREQ(strings[i * N_PER_PROPERTY + 2], &descr->value);
    
    const String *cat = fiftyoneDegreesPropertyGetCategory(stringsCollection, p, &item, exception);
    EXPECT_STREQ(strings[i * N_PER_PROPERTY + 3], &cat->value);
    
    const String *url = fiftyoneDegreesPropertyGetUrl(stringsCollection, p, &item, exception);
    EXPECT_STREQ(strings[i*N_PER_PROPERTY + 4], &url->value);
}

/**
 * Check a property with no URL gives an empty string rather than the string at
 * offset 0, and that the item can be released afterwards.
 */
TEST_F(PropertyTests, PropertyWithNoUrl) {
    EXCEPTION_CREATE
    const fiftyoneDegreesProperty noUrl = {};
    DataReset(&item.data);
    const String *url = fiftyoneDegreesPropertyGetUrl(
        stringsCollection, &noUrl, &item, exception);
    ASSERT_TRUE(EXCEPTION_OKAY);
    ASSERT_NE(url, nullptr);
    EXPECT_STREQ("", &url->value);
    COLLECTION_RELEASE(item.collection, &item);
}

TEST_F(PropertyTests, RetrievePropertyFromCollection) {
    
    for (int i=0;i<N_PROPERTIES;i++) {
        EXCEPTION_CREATE
        EXPECT_EQ(types[i], fiftyoneDegreesPropertyGetValueType(propertiesCollection, i, exception));
        {
            fiftyoneDegreesProperty *p = fiftyoneDegreesPropertyGet(propertiesCollection, i, &item, exception);
            assessProperty(p, i);
        }
        {
            const Property *p = fiftyoneDegreesPropertyGetByName(propertiesCollection, stringsCollection, strings[i * N_PER_PROPERTY + 1], &item, exception);
            assessProperty(p, i);
        }
    }
}

/**
 * The name of each value type, as a single value and as a list. Only a string
 * list has a name of its own. The types without a name, and numbers that are
 * not property value types, are named as a string.
 */
TEST(PropertyValueTypeTests, Names) {
    const struct {
        int type;
        const char *name;
        const char *listName;
    } cases[] = {
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING, "string", "string[]" },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER, "int", "int" },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DOUBLE, "double", "double" },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_BOOLEAN, "bool", "bool" },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_JAVASCRIPT,
            "javascript",
            "javascript"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT,
            "single",
            "single"
        },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_BYTE, "string", "string" },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_COORDINATE,
            "coordinate",
            "coordinate"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_IP_ADDRESS,
            "ipaddress",
            "ipaddress"
        },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB, "wkb", "wkb" },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_OBJECT, "string", "string" },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION,
            "string",
            "string"
        },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH, "string", "string" },
        { FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB_R, "wkbr", "wkbr" },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_STRING,
            "weightedstring",
            "weightedstring"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_INT,
            "weightedint",
            "weightedint"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_DOUBLE,
            "weighteddouble",
            "weighteddouble"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_BOOL,
            "weightedbool",
            "weightedbool"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_SINGLE,
            "weightedsingle",
            "weightedsingle"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_BYTE,
            "weightedbyte",
            "weightedbyte"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_IP_ADDRESS,
            "weightedipaddress",
            "weightedipaddress"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_WKB_R,
            "weightedwkbr",
            "weightedwkbr"
        },
        {
            FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WEIGHTED_WKB_R + 1,
            "string",
            "string"
        },
        // The largest value type a property record can hold.
        { UINT8_MAX, "string", "string" },
        {
            FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_PROPERTY,
            "string",
            "string"
        },
    };
    for (const auto &c : cases) {
        const fiftyoneDegreesPropertyValueType type =
            (fiftyoneDegreesPropertyValueType)c.type;
        EXPECT_STREQ(
            c.name,
            fiftyoneDegreesPropertyValueTypeGetName(type, false)) << c.type;
        EXPECT_STREQ(
            c.listName,
            fiftyoneDegreesPropertyValueTypeGetName(type, true)) << c.type;
    }
}
