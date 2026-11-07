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
 * @file HashDefaultValueTests.cpp
 * @brief Checks the default value the metadata gives every property of each
 * data file found, and pins the defaults of properties whose values are
 * numbers, flags and names, all of which Hash data files store as strings.
 *
 * The text of each default is written again here from the value at the
 * property's default value index, and the default value must be the one the
 * values collection finds by that text.
 */

#include <memory>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/fiftyone.h"

using std::string;
using std::unique_ptr;
using std::vector;
namespace Hash = FiftyoneDegrees::DeviceDetection::Hash;
namespace Common = FiftyoneDegrees::Common;

using Properties = Common::Collection<string, Common::PropertyMetaData>;
using Values =
	Common::Collection<Common::ValueMetaDataKey, Common::ValueMetaData>;

/** Bytes that hold the text of any default value. */
static const size_t defaultValueTextBytes = 1024 * 1024;

/**
 * The default the metadata gives a property, as text and as the name of the
 * value found, which is nullptr when no value is found.
 */
struct HashPinnedDefault {
	const char *property;
	const char *text;
	const char *value;
};

/**
 * Defaults of flags, numbers and a name, checked in each data file that has
 * the property. A property with no default value index has an empty default
 * text, and no default value when none of its values has an empty name.
 */
static const HashPinnedDefault hashPinnedDefaults[] = {
	{ "IsMobile", "False", "False" },
	{ "CookiesCapable", "True", "True" },
	{ "ScreenPixelsWidth", "Unknown", "Unknown" },
	{ "ScreenInchesDiagonal", "Unknown", "Unknown" },
	{ "HardwareRank", "0", "0" },
	{ "PlatformVendor", "", nullptr },
};

/**
 * A manager over every property of a data file, with the least memory, which
 * the metadata shares and which is freed with it.
 */
static std::shared_ptr<fiftyoneDegreesResourceManager> openDefaults(
	const string &path) {
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesResourceManager *manager =
		new fiftyoneDegreesResourceManager();
	EXCEPTION_CREATE;
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesHashInitManagerFromFile(
			manager,
			&config,
			&properties,
			path.c_str(),
			exception);
	if (status != FIFTYONE_DEGREES_STATUS_SUCCESS || EXCEPTION_FAILED) {
		delete manager;
		throw Common::StatusCodeException(status);
	}
	return std::shared_ptr<fiftyoneDegreesResourceManager>(
		manager,
		[](fiftyoneDegreesResourceManager *m) {
			fiftyoneDegreesResourceManagerFree(m);
			delete m;
		});
}

/** The data set of a manager for the length of a scope. */
class HashDefaultValueDataSet {
public:
	explicit HashDefaultValueDataSet(fiftyoneDegreesResourceManager *manager)
		: dataSet(fiftyoneDegreesDataSetHashGet(manager)) {}
	~HashDefaultValueDataSet() {
		fiftyoneDegreesDataSetHashRelease(dataSet);
	}
	fiftyoneDegreesDataSetHash *dataSet;
};

/**
 * The name of the value at the property's default value index, read as the
 * string every Hash value is stored as, or empty when it has no default value
 * index.
 */
static string writtenDefault(
	const fiftyoneDegreesDataSetHash *dataSet,
	const fiftyoneDegreesProperty &property) {
	if (property.defaultValueIndex == UINT32_MAX) {
		return string();
	}
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem valueItem;
	fiftyoneDegreesDataReset(&valueItem.data);
	const fiftyoneDegreesValue *value = fiftyoneDegreesValueGet(
		dataSet->values,
		property.defaultValueIndex,
		&valueItem,
		exception);
	EXCEPTION_THROW;
	const fiftyoneDegreesPropertyValueType type =
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING;
	fiftyoneDegreesCollectionItem nameItem;
	fiftyoneDegreesDataReset(&nameItem.data);
	const fiftyoneDegreesStoredBinaryValue *name =
		fiftyoneDegreesStoredBinaryValueGet(
			dataSet->strings,
			(uint32_t)value->nameOffset,
			type,
			&nameItem,
			exception);
	EXCEPTION_THROW;
	vector<char> buffer(defaultValueTextBytes);
	fiftyoneDegreesStringBuilder builder = { buffer.data(), buffer.size() };
	fiftyoneDegreesStringBuilderInit(&builder);
	fiftyoneDegreesStringBuilderAddStringValue(
		&builder,
		name,
		type,
		FIFTYONE_DEGREES_MAX_DOUBLE_DECIMAL_PLACES,
		exception);
	fiftyoneDegreesStringBuilderComplete(&builder);
	EXCEPTION_THROW;
	EXPECT_FALSE(builder.full);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &nameItem);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &valueItem);
	return string(buffer.data());
}

class HashDefaultValueTests : public Base {
protected:
	/**
	 * Checks every property of the data file and returns the number of
	 * defaults found.
	 */
	int checkEveryProperty(const string &path) {
		int found = 0;
		const auto manager = openDefaults(path);
		Hash::MetaDataHash metaData(manager);
		HashDefaultValueDataSet scope(manager.get());
		const fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		unique_ptr<Properties> properties(metaData.getProperties());
		unique_ptr<Values> values(metaData.getValues());
		EXPECT_EQ(
			fiftyoneDegreesCollectionGetCount(dataSet->properties),
			properties->getSize());
		for (uint32_t i = 0; i < properties->getSize(); i++) {
			EXCEPTION_CREATE;
			unique_ptr<Common::PropertyMetaData> metaDataProperty(
				properties->getByIndex(i));
			const string &name = metaDataProperty->getName();
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			const fiftyoneDegreesProperty *stored =
				fiftyoneDegreesPropertyGet(
					dataSet->properties,
					i,
					&item,
					exception);
			EXCEPTION_THROW;
			const fiftyoneDegreesProperty property = *stored;
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);

			// The text is the name of the value at the default value index.
			EXPECT_EQ(
				writtenDefault(dataSet, property),
				metaDataProperty->getDefaultValue()) << name;

			// The default is the value the values collection finds by that
			// text, which is the value at the index the C library finds.
			unique_ptr<Common::ValueMetaData> byText(values->getByKey(
				Common::ValueMetaDataKey(
					name,
					metaDataProperty->getDefaultValue())));
			unique_ptr<Common::ValueMetaData> actual(
				metaData.getDefaultValueForProperty(metaDataProperty.get()));
			// Hash data sets have no property types.
			const long index = fiftyoneDegreesValueGetDefaultIndex(
				dataSet->values,
				dataSet->strings,
				nullptr,
				&property,
				exception);
			EXCEPTION_THROW;
			EXPECT_EQ(byText == nullptr, actual == nullptr) << name;
			EXPECT_EQ(actual == nullptr, index < 0) << name;
			if (actual != nullptr && byText != nullptr && index >= 0) {
				EXPECT_EQ(byText->getName(), actual->getName()) << name;
				EXPECT_EQ(name, actual->getKey().getPropertyName());
				EXPECT_GE((uint32_t)index, property.firstValueIndex) << name;
				EXPECT_LE((uint32_t)index, property.lastValueIndex) << name;
				unique_ptr<Common::ValueMetaData> atIndex(
					values->getByIndex((uint32_t)index));
				EXPECT_EQ(atIndex->getName(), actual->getName()) << name;
				found++;
			}

			for (const HashPinnedDefault &pin : hashPinnedDefaults) {
				if (name == pin.property) {
					EXPECT_EQ(pin.text, metaDataProperty->getDefaultValue()) <<
						name;
					if (pin.value == nullptr) {
						EXPECT_TRUE(actual == nullptr) << name;
					}
					else if (actual != nullptr) {
						EXPECT_EQ(pin.value, actual->getName()) << name;
					}
					else {
						ADD_FAILURE() << name << " has no default value";
					}
				}
			}
		}
		return found;
	}
};

TEST_F(HashDefaultValueTests, EveryPropertyOfEachDataFile) {
	bool any = false;
	for (int i = 0; i < _HashFileNamesLength; i++) {
		const string path = GetFilePath(_dataFolderName, _HashFileNames[i]);
		if (path.empty()) {
			continue;
		}
		any = true;
		SCOPED_TRACE(path);
		EXPECT_GT(checkEveryProperty(path), 0) <<
			"Every data file has a property with a default.";
	}
	if (any == false) {
		GTEST_SKIP() << "No Device Detection data file was found.";
	}
}
