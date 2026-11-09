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
 * @file IpiDefaultValueTests.cpp
 * @brief Checks the default value the metadata gives every property of each
 * data file found, and pins the defaults of numbers, areas and strings.
 *
 * The text of each default is written again here from the value at the
 * property's default value index, with numbers to 15 places, and the default
 * value must be the one the values collection finds by that text. Every
 * string value must also be found by its name.
 */

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using std::string;
using std::unique_ptr;
using std::vector;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

using Properties = Common::Collection<string, Common::PropertyMetaData>;
using Values =
	Common::Collection<Common::ValueMetaDataKey, Common::ValueMetaData>;

/** The free ASN data file, which the data repository's script downloads. */
static const char defaultValueAsnFileName[] =
	"51Degrees-IPIV4AsnIpiV41.ipi";

/** Bytes that hold the text of any default value. */
static const size_t defaultValueTextBytes = 1024 * 1024;

/**
 * The default the metadata gives a property, as text and as the name of the
 * value found, which is nullptr when no value is found.
 */
struct IpiPinnedDefault {
	const char *property;
	const char *text;
	const char *value;
};

/**
 * Defaults of integers, a latitude and a longitude, an area and strings,
 * checked in each data file that has the property. A number is written to 15
 * places as its default text and to 3 as the name of the value found.
 */
static const IpiPinnedDefault ipiPinnedDefaults[] = {
	{ "AccuracyRadiusMax", "0", "0" },
	{ "HumanProbability", "-1", "-1" },
	{ "Latitude", "0", "0" },
	{ "Longitude", "0", "0" },
	{ "Areas", "POLYGON EMPTY", "POLYGON EMPTY" },
	{ "Asn", "Unknown", "Unknown" },
	{ "IsTor", "False", "False" },
};

/** The kinds of value a property is stored as. */
enum class IpiValueKind { String, Number, Area, IpAddress, Other };

static IpiValueKind kindOf(const fiftyoneDegreesPropertyValueType type) {
	switch (type) {
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING:
		return IpiValueKind::String;
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER:
	case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT:
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH:
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION:
	case FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_BYTE:
		return IpiValueKind::Number;
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB:
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB_R:
		return IpiValueKind::Area;
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_IP_ADDRESS:
		return IpiValueKind::IpAddress;
	default:
		return IpiValueKind::Other;
	}
}

/**
 * A manager over every property of a data file, with the least memory, which
 * the metadata shares and which is freed with it.
 */
static std::shared_ptr<fiftyoneDegreesResourceManager> openDefaults(
	const string &path) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesResourceManager *manager =
		new fiftyoneDegreesResourceManager();
	EXCEPTION_CREATE;
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesIpiInitManagerFromFile(
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
class IpiDefaultValueDataSet {
public:
	explicit IpiDefaultValueDataSet(fiftyoneDegreesResourceManager *manager)
		: dataSet(fiftyoneDegreesDataSetIpiGet(manager)) {}
	~IpiDefaultValueDataSet() {
		fiftyoneDegreesDataSetIpiRelease(dataSet);
	}
	fiftyoneDegreesDataSetIpi *dataSet;
};

/**
 * The text of the value at the property's default value index, written with
 * the C library's string builder as the type the value's property is stored
 * as, with numbers to 15 places, or empty when it has no default value index.
 */
static string writtenDefault(
	const fiftyoneDegreesDataSetIpi *dataSet,
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
		fiftyoneDegreesPropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			(uint32_t)value->propertyIndex,
			exception);
	EXCEPTION_THROW;
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

/**
 * Names of a weighted list checked by name. Its values are not in name order,
 * so each name the binary search misses reads every value of the property,
 * and StateGeoNameIdsGeographical has more than a million.
 */
static const size_t weightedNamesChecked = 16;

/** The name of a value stored as a string, and whether it has a weight. */
static string stringValueName(
	const fiftyoneDegreesDataSetIpi *dataSet,
	const uint32_t index,
	bool *weighted) {
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem valueItem;
	fiftyoneDegreesDataReset(&valueItem.data);
	const fiftyoneDegreesValue *value = fiftyoneDegreesValueGet(
		dataSet->values,
		index,
		&valueItem,
		exception);
	EXCEPTION_THROW;
	if (weighted != nullptr) {
		*weighted = fiftyoneDegreesValueIsWeighted(value);
	}
	fiftyoneDegreesCollectionItem nameItem;
	fiftyoneDegreesDataReset(&nameItem.data);
	const fiftyoneDegreesString *name = fiftyoneDegreesValueGetName(
		dataSet->strings,
		value,
		&nameItem,
		exception);
	EXCEPTION_THROW;
	const string result(&name->value);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &nameItem);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &valueItem);
	return result;
}

/**
 * Looks up each distinct name of every string property's values, the first
 * weightedNamesChecked of a weighted list, and expects a value of the
 * property with that name. Returns the number of names looked up.
 */
static size_t checkValuesByName(const string &path) {
	size_t checked = 0;
	const auto manager = openDefaults(path);
	IpiDefaultValueDataSet scope(manager.get());
	const fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
	const uint32_t count =
		fiftyoneDegreesCollectionGetCount(dataSet->properties);
	for (uint32_t i = 0; i < count; i++) {
		EXCEPTION_CREATE;
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesProperty *stored = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			i,
			&item,
			exception);
		EXCEPTION_THROW;
		const fiftyoneDegreesProperty property = *stored;
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		const fiftyoneDegreesPropertyValueType type =
			fiftyoneDegreesPropertyGetStoredType(
				dataSet->propertyTypes,
				&property,
				exception);
		EXCEPTION_THROW;
		if (type != FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING ||
			(int32_t)property.firstValueIndex == -1) {
			continue;
		}
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesString *propertyName =
			fiftyoneDegreesPropertyGetName(
				dataSet->strings,
				&property,
				&item,
				exception);
		EXCEPTION_THROW;
		const string name(&propertyName->value);
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &item);

		bool weighted = false;
		stringValueName(dataSet, property.firstValueIndex, &weighted);
		std::set<string> seen;
		for (uint32_t v = property.firstValueIndex;
			v <= property.lastValueIndex &&
			(weighted == false || seen.size() < weightedNamesChecked);
			v++) {
			const string valueName = stringValueName(dataSet, v, nullptr);
			if (seen.insert(valueName).second == false) {
				continue;
			}
			checked++;
			const long found = fiftyoneDegreesValueGetIndexByNameAndType(
				dataSet->values,
				dataSet->strings,
				&property,
				FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING,
				valueName.c_str(),
				exception);
			EXCEPTION_THROW;
			if (found < (long)property.firstValueIndex ||
				found > (long)property.lastValueIndex) {
				ADD_FAILURE() << name << " '" << valueName <<
					"', the value at " << v << ", was not found";
				continue;
			}
			EXPECT_EQ(
				valueName,
				stringValueName(dataSet, (uint32_t)found, nullptr)) << name;
		}
	}
	return checked;
}

class IpiDefaultValueTests : public Base {
protected:
	/**
	 * Checks every property of the data file and returns the number of
	 * defaults found of each kind.
	 */
	vector<int> checkEveryProperty(const string &path) {
		vector<int> found((size_t)IpiValueKind::Other + 1, 0);
		const auto manager = openDefaults(path);
		Ipi::MetaDataIpi metaData(manager);
		IpiDefaultValueDataSet scope(manager.get());
		const fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
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
			const fiftyoneDegreesPropertyValueType type =
				fiftyoneDegreesPropertyGetStoredType(
					dataSet->propertyTypes,
					&property,
					exception);
			EXCEPTION_THROW;

			// The text is the value at the default value index to 15 places.
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
			const long index = fiftyoneDegreesValueGetDefaultIndex(
				dataSet->values,
				dataSet->strings,
				dataSet->propertyTypes,
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
				found[(size_t)kindOf(type)]++;
			}

			for (const IpiPinnedDefault &pin : ipiPinnedDefaults) {
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

TEST_F(IpiDefaultValueTests, EveryPropertyOfEachDataFile) {
	vector<string> paths;
	for (int i = 0; i < _IpiFileNamesLength; i++) {
		paths.push_back(GetFilePath(_dataFolderName, _IpiFileNames[i]));
	}
	paths.push_back(GetFilePath(_dataFolderName, defaultValueAsnFileName));
	bool any = false;
	for (const string &path : paths) {
		if (path.empty()) {
			continue;
		}
		any = true;
		// The file is printed rather than added with SCOPED_TRACE, whose
		// memory the base class's memory check counts.
		printf("Checking %s\n", path.c_str());
		const vector<int> found = checkEveryProperty(path);
		EXPECT_GT(found[(size_t)IpiValueKind::String], 0) <<
			"Every data file has a string with a default.";
		if (path.find(_IpiFileNames[0]) != string::npos) {
			// The Enterprise file has defaults of every kind.
			EXPECT_GT(found[(size_t)IpiValueKind::Number], 0);
			EXPECT_GT(found[(size_t)IpiValueKind::Area], 0);
			EXPECT_GT(found[(size_t)IpiValueKind::IpAddress], 0);
		}
	}
	if (any == false) {
		GTEST_SKIP() << "No IP Intelligence data file was found.";
	}
}

TEST_F(IpiDefaultValueTests, EveryStringValueFoundByName) {
	vector<string> paths;
	for (int i = 0; i < _IpiFileNamesLength; i++) {
		paths.push_back(GetFilePath(_dataFolderName, _IpiFileNames[i]));
	}
	paths.push_back(GetFilePath(_dataFolderName, defaultValueAsnFileName));
	bool any = false;
	for (const string &path : paths) {
		if (path.empty()) {
			continue;
		}
		any = true;
		printf("Checking %s\n", path.c_str());
		const size_t checked = checkValuesByName(path);
		printf("%zu names looked up\n", checked);
		EXPECT_GT(checked, (size_t)0) << path;
	}
	if (any == false) {
		GTEST_SKIP() << "No IP Intelligence data file was found.";
	}
}
