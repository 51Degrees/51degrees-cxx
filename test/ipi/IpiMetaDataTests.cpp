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
 * @file IpiMetaDataTests.cpp
 * @brief Checks the metadata the IP Intelligence C functions give for every
 * property and value of each data file found against
 * the data set's own records, and that the C++ metadata gives the same
 * answers.
 *
 * Each text is read twice, into a buffer that holds it and into one too
 * small, so both the written and the allocated text are checked.
 */

#include <memory>
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
static const char ipiMetaDataAsnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** Bytes that hold most texts, so the C functions write them in place. */
static const size_t ipiMetaDataTextBytes = 1024;

/** Bytes that hold the text of any value, an area included. */
static const size_t ipiMetaDataWrittenBytes = 32 * 1024 * 1024;

/** Bytes too few for most texts, so the C functions allocate the text. */
static const size_t ipiMetaDataShortBytes = 2;

/** Every value of a property with at most this many is checked. */
static const uint32_t ipiMetaDataAllValues = 500;

/** The values of a larger property checked, spread across them. */
static const uint32_t ipiMetaDataSampledValues = 200;

/** A name no property or value has. */
static const char *ipiMetaDataNoName = "NoSuchName51Degrees";

/**
 * A manager over the properties named, or all when none are, with the least
 * memory so the collections read the file, which the metadata shares and
 * which is freed with it.
 */
static std::shared_ptr<fiftyoneDegreesResourceManager> openIpiMetaData(
	const string &path,
	const char *requiredProperties) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = requiredProperties;
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
class IpiMetaDataDataSet {
public:
	explicit IpiMetaDataDataSet(fiftyoneDegreesResourceManager *manager)
		: dataSet(fiftyoneDegreesDataSetIpiGet(manager)) {}
	~IpiMetaDataDataSet() {
		fiftyoneDegreesDataSetIpiRelease(dataSet);
	}
	fiftyoneDegreesDataSetIpi *dataSet;
};

/**
 * A text a C function gave, freed when it is not the buffer. The exception
 * must not be set when the text is not NULL.
 */
static string ipiMetaDataTake(
	char *text,
	const char *buffer,
	fiftyoneDegreesException *exception) {
	EXPECT_TRUE(text == nullptr || EXCEPTION_OKAY);
	string result = text == nullptr ? string("<null>") : string(text);
	if (text != nullptr && text != buffer) {
		fiftyoneDegreesFree(text);
	}
	return result;
}

/** The string at the offset in the strings collection, as stored. */
static string ipiMetaDataStored(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t offset) {
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesString *value = fiftyoneDegreesStringGet(
		dataSet->strings,
		offset,
		&item,
		exception);
	EXCEPTION_THROW;
	string result = value == nullptr ? string() : string(&value->value);
	if (value != nullptr) {
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &item);
	}
	return result;
}

/**
 * The value's name written as the type its property is stored as, with a
 * number to FIFTYONE_DEGREES_WKT_DECIMAL_PLACES places, or empty when the
 * name cannot be written.
 */
static string ipiMetaDataWrittenName(
	fiftyoneDegreesDataSetIpi *dataSet,
	const fiftyoneDegreesValue &value,
	vector<char> &buffer) {
	EXCEPTION_CREATE;
	const fiftyoneDegreesPropertyValueType type =
		fiftyoneDegreesPropertyGetStoredTypeByIndex(
			dataSet->propertyTypes,
			(uint32_t)value.propertyIndex,
			exception);
	EXCEPTION_THROW;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesStoredBinaryValue *name =
		fiftyoneDegreesStoredBinaryValueGet(
			dataSet->strings,
			(uint32_t)value.nameOffset,
			type,
			&item,
			exception);
	EXCEPTION_THROW;
	if (name == nullptr) {
		return string();
	}
	fiftyoneDegreesStringBuilder builder = { buffer.data(), buffer.size() };
	fiftyoneDegreesStringBuilderInit(&builder);
	fiftyoneDegreesStringBuilderAddStringValue(
		&builder,
		name,
		type,
		FIFTYONE_DEGREES_WKT_DECIMAL_PLACES,
		exception);
	fiftyoneDegreesStringBuilderComplete(&builder);
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &item);
	EXPECT_FALSE(builder.full);
	return EXCEPTION_OKAY ? string(buffer.data()) : string();
}

/** A copy of the property at the index, as stored. */
static fiftyoneDegreesProperty ipiMetaDataStoredProperty(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t index) {
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesProperty *stored = fiftyoneDegreesPropertyGet(
		dataSet->properties,
		index,
		&item,
		exception);
	EXCEPTION_THROW;
	const fiftyoneDegreesProperty property = *stored;
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
	return property;
}

/** A copy of the value at the index, as stored. */
static fiftyoneDegreesValue ipiMetaDataStoredValue(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t index) {
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesValue *stored = fiftyoneDegreesValueGet(
		dataSet->values,
		index,
		&item,
		exception);
	EXCEPTION_THROW;
	const fiftyoneDegreesValue value = *stored;
	FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &item);
	return value;
}

/** A text of the property, read with a buffer of the length given. */
static string ipiMetaDataPropertyText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t index,
	fiftyoneDegreesPropertyText text,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesIpiGetPropertyText(
		dataSet,
		index,
		text,
		buffer.data(),
		buffer.size(),
		exception);
	return ipiMetaDataTake(result, buffer.data(), exception);
}

/** The name of a map of the property, read with a buffer of the length. */
static string ipiMetaDataMapText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t index,
	uint32_t mapIndex,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesIpiGetPropertyMapText(
		dataSet,
		index,
		mapIndex,
		buffer.data(),
		buffer.size(),
		exception);
	return ipiMetaDataTake(result, buffer.data(), exception);
}

/** A text of the value, read with the buffer given. */
static string ipiMetaDataValueText(
	fiftyoneDegreesDataSetIpi *dataSet,
	uint32_t index,
	fiftyoneDegreesValueText text,
	vector<char> &buffer,
	size_t length) {
	EXCEPTION_CREATE;
	char *result = fiftyoneDegreesIpiGetValueText(
		dataSet,
		index,
		text,
		buffer.data(),
		length,
		exception);
	return ipiMetaDataTake(result, buffer.data(), exception);
}

/** The default value text the C library writes for the property. */
static string ipiMetaDataDefaultText(
	fiftyoneDegreesDataSetIpi *dataSet,
	const fiftyoneDegreesProperty &property,
	vector<char> &buffer) {
	EXCEPTION_CREATE;
	char *result = fiftyoneDegreesValueGetDefaultText(
		dataSet->values,
		dataSet->strings,
		dataSet->propertyTypes,
		&property,
		buffer.data(),
		buffer.size(),
		exception);
	return ipiMetaDataTake(result, buffer.data(), exception);
}

class IpiMetaDataTests : public Base {
protected:
	/** Runs the check on each data file found, or skips the test. */
	template <typename Check>
	void eachDataFile(Check check) {
		vector<string> paths;
		for (int i = 0; i < _IpiFileNamesLength; i++) {
			paths.push_back(GetFilePath(_dataFolderName, _IpiFileNames[i]));
		}
		paths.push_back(GetFilePath(_dataFolderName, ipiMetaDataAsnFileName));
		bool any = false;
		for (const string &path : paths) {
			if (path.empty()) {
				continue;
			}
			any = true;
			// The file is printed rather than added with SCOPED_TRACE, whose
			// memory the base class's memory check counts.
			printf("Checking %s\n", path.c_str());
			check(path);
		}
		if (any == false) {
			GTEST_SKIP() << "No IP Intelligence data file was found.";
		}
	}

	/** Checks every property's fields, texts and maps. */
	void checkProperties(const string &path) {
		const auto manager = openIpiMetaData(path, nullptr);
		Ipi::MetaDataIpi metaData(manager);
		IpiMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
		vector<char> buffer(ipiMetaDataTextBytes);
		unique_ptr<Properties> properties(metaData.getProperties());
		const uint32_t count = fiftyoneDegreesIpiGetPropertyCount(dataSet);
		EXPECT_EQ(
			fiftyoneDegreesCollectionGetCount(dataSet->properties),
			count);
		EXPECT_EQ(count, properties->getSize());
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			const fiftyoneDegreesProperty stored =
				ipiMetaDataStoredProperty(dataSet, i);
			const string name = ipiMetaDataStored(
				dataSet,
				stored.nameOffset);

			uint8_t valueType = 0, componentId = 0, displayOrder = 0;
			bool isMandatory = false, isList = false, isObsolete = false;
			bool show = false, showValues = false, isAvailable = false;
			uint32_t mapCount = 0;
			int requiredIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesIpiGetProperty(
				dataSet,
				i,
				&valueType,
				&componentId,
				&displayOrder,
				&isMandatory,
				&isList,
				&isObsolete,
				&show,
				&showValues,
				&mapCount,
				&isAvailable,
				&requiredIndex,
				exception)) << name;
			ASSERT_TRUE(EXCEPTION_OKAY) << name;
			EXPECT_EQ(stored.valueType, valueType) << name;
			EXPECT_EQ(
				((fiftyoneDegreesComponent*)dataSet->componentsList.items[
					stored.componentIndex].data.ptr)->componentId,
				componentId) << name;
			EXPECT_EQ(stored.displayOrder, displayOrder) << name;
			EXPECT_EQ(stored.isMandatory != 0, isMandatory) << name;
			EXPECT_EQ(stored.isList != 0, isList) << name;
			EXPECT_EQ(stored.isObsolete != 0, isObsolete) << name;
			EXPECT_EQ(stored.show != 0, show) << name;
			EXPECT_EQ(stored.showValues != 0, showValues) << name;
			EXPECT_EQ(stored.mapCount, mapCount) << name;
			EXPECT_EQ(
				fiftyoneDegreesPropertiesGetPropertyIndexFromName(
					dataSet->b.b.available,
					name.c_str()) >= 0,
				isAvailable) << name;
			EXPECT_EQ(
				fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
					dataSet->b.b.available,
					name.c_str()),
				requiredIndex) << name;

			// The first property with the name is found by it.
			const int found = fiftyoneDegreesIpiGetPropertyIndex(
				dataSet,
				name.c_str(),
				exception);
			EXPECT_TRUE(EXCEPTION_OKAY) << name;
			ASSERT_GE(found, 0) << name;
			EXPECT_LE((uint32_t)found, i) << name;

			const string description =
				(int32_t)stored.descriptionOffset == -1
				? string()
				: ipiMetaDataStored(dataSet, stored.descriptionOffset);
			const string category = (int32_t)stored.categoryOffset == -1
				? string()
				: ipiMetaDataStored(dataSet, stored.categoryOffset);
			const string url = (int32_t)stored.urlOffset <= 0
				? string()
				: ipiMetaDataStored(dataSet, stored.urlOffset);
			const string defaultValue =
				ipiMetaDataDefaultText(dataSet, stored, buffer);
			for (size_t length : {
				ipiMetaDataTextBytes,
				ipiMetaDataShortBytes }) {
				EXPECT_EQ(name, ipiMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
					length)) << name;
				EXPECT_EQ(description, ipiMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION,
					length)) << name;
				EXPECT_EQ(category, ipiMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY,
					length)) << name;
				EXPECT_EQ(url, ipiMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_URL,
					length)) << name;
				EXPECT_EQ(defaultValue, ipiMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_DEFAULT_VALUE,
					length)) << name;
			}

			vector<string> maps;
			for (uint32_t m = 0; m < stored.mapCount; m++) {
				const uint32_t offset =
					(uint32_t)fiftyoneDegreesCollectionGetInteger32(
						dataSet->maps,
						stored.firstMapIndex + m,
						exception);
				EXCEPTION_THROW;
				const string map = ipiMetaDataStored(dataSet, offset);
				maps.push_back(map);
				EXPECT_EQ(map, ipiMetaDataMapText(
					dataSet,
					i,
					m,
					ipiMetaDataTextBytes)) << name;
				EXPECT_EQ(map, ipiMetaDataMapText(
					dataSet,
					i,
					m,
					ipiMetaDataShortBytes)) << name;
			}

			uint32_t firstValueIndex = 0, valueCount = 0;
			long defaultValueIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesIpiGetPropertyValueIndexes(
				dataSet,
				i,
				&firstValueIndex,
				&valueCount,
				&defaultValueIndex,
				exception)) << name;
			EXPECT_EQ(stored.firstValueIndex, firstValueIndex) << name;
			EXPECT_EQ(
				(int32_t)stored.firstValueIndex == -1
				? 0
				: stored.lastValueIndex - stored.firstValueIndex + 1,
				valueCount) << name;
			EXPECT_EQ(
				fiftyoneDegreesValueGetDefaultIndex(
					dataSet->values,
					dataSet->strings,
					dataSet->propertyTypes,
					&stored,
					exception),
				defaultValueIndex) << name;

			// The C++ metadata gives the same answers.
			unique_ptr<Common::PropertyMetaData> property(
				properties->getByIndex(i));
			ASSERT_TRUE(property != nullptr) << name;
			EXPECT_EQ(name, property->getName());
			EXPECT_EQ(description, property->getDescription()) << name;
			EXPECT_EQ(category, property->getCategory()) << name;
			EXPECT_EQ(url, property->getUrl()) << name;
			EXPECT_EQ(defaultValue, property->getDefaultValue()) << name;
			EXPECT_EQ(isAvailable, property->getAvailable()) << name;
			EXPECT_EQ(componentId, property->getComponentId()) << name;
			EXPECT_EQ(maps, property->getDataFilesWherePresent()) << name;
			unique_ptr<Common::PropertyMetaData> byName(
				properties->getByKey(name));
			ASSERT_TRUE(byName != nullptr) << name;
			EXPECT_EQ(name, byName->getName());
		}
	}

	/** Checks the values of every property. */
	void checkValues(const string &path) {
		const auto manager = openIpiMetaData(path, nullptr);
		Ipi::MetaDataIpi metaData(manager);
		IpiMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
		vector<char> buffer(ipiMetaDataTextBytes);
		vector<char> written(ipiMetaDataWrittenBytes);
		unique_ptr<Properties> properties(metaData.getProperties());
		const uint32_t count = fiftyoneDegreesIpiGetPropertyCount(dataSet);
		uint32_t foundAtIndex = 0;
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			uint32_t first = 0, valueCount = 0;
			ASSERT_TRUE(fiftyoneDegreesIpiGetPropertyValueIndexes(
				dataSet,
				i,
				&first,
				&valueCount,
				nullptr,
				exception));
			const fiftyoneDegreesProperty stored =
				ipiMetaDataStoredProperty(dataSet, i);
			const fiftyoneDegreesPropertyValueType storedType =
				fiftyoneDegreesPropertyGetStoredType(
					dataSet->propertyTypes,
					&stored,
					exception);
			EXCEPTION_THROW;
			unique_ptr<Common::PropertyMetaData> property(
				properties->getByIndex(i));
			const string &name = property->getName();
			unique_ptr<Values> values(
				metaData.getValuesForProperty(property.get()));
			EXPECT_EQ(valueCount, values->getSize()) << name;
			const uint32_t step = valueCount <= ipiMetaDataAllValues
				? 1
				: valueCount / ipiMetaDataSampledValues;
			for (uint32_t v = 0; v < valueCount; v += step) {
				const uint32_t valueIndex = first + v;
				const fiftyoneDegreesValue value =
					ipiMetaDataStoredValue(dataSet, valueIndex);
				uint32_t propertyIndex = UINT32_MAX;
				ASSERT_TRUE(fiftyoneDegreesIpiGetValue(
					dataSet,
					valueIndex,
					&propertyIndex,
					exception)) << name;
				EXPECT_EQ(i, propertyIndex) << name;
				const string valueName =
					ipiMetaDataWrittenName(dataSet, value, written);
				// A weighted value's URL offset holds its weight.
				const string url = value.urlOffsetOrWeight <= 0 ||
					fiftyoneDegreesValueIsWeighted(&value)
					? string()
					: ipiMetaDataStored(
						dataSet,
						(uint32_t)value.urlOffsetOrWeight);
				for (size_t length : {
					buffer.size(),
					ipiMetaDataShortBytes }) {
					EXPECT_EQ(valueName, ipiMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_NAME,
						buffer,
						length)) << name;
					EXPECT_EQ(string(), ipiMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_DESCRIPTION,
						buffer,
						length)) << name;
					EXPECT_EQ(url, ipiMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_URL,
						buffer,
						length)) << name;
				}

				// The C++ metadata gives the same answers.
				unique_ptr<Common::ValueMetaData> metaDataValue(
					values->getByIndex(v));
				ASSERT_TRUE(metaDataValue != nullptr) << name;
				EXPECT_EQ(name, metaDataValue->getKey().getPropertyName());
				EXPECT_EQ(valueName, metaDataValue->getName()) << name;
				EXPECT_EQ(string(), metaDataValue->getDescription()) << name;
				EXPECT_EQ(url, metaDataValue->getUrl()) << name;

				// A number written to fewer places than the search compares
				// it to is not found by its text, so only a string is. The
				// search relies on the order the data file keeps a
				// property's values in, so a value out of that order, or one
				// sharing its name with another, can be found elsewhere or
				// not at all. Whatever is found must have the name.
				if (storedType == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING) {
					const long found = fiftyoneDegreesIpiGetValueIndex(
						dataSet,
						i,
						valueName.c_str(),
						exception);
					EXPECT_TRUE(EXCEPTION_OKAY) << name;
					unique_ptr<Common::ValueMetaData> byKey(
						values->getByKey(metaDataValue->getKey()));
					EXPECT_EQ(found >= 0, byKey != nullptr) << name;
					if (found >= 0) {
						EXPECT_EQ(valueName, ipiMetaDataValueText(
							dataSet,
							(uint32_t)found,
							FIFTYONE_DEGREES_VALUE_TEXT_NAME,
							buffer,
							buffer.size())) << name;
						EXPECT_EQ(valueName, byKey->getName()) << name;
					}
					foundAtIndex += found == (long)valueIndex ? 1 : 0;
				}
			}
		}
		EXPECT_GT(foundAtIndex, 0u) << "No string value was found by name.";
	}
};

TEST_F(IpiMetaDataTests, PropertiesOfEachDataFile) {
	eachDataFile([this](const string &path) { checkProperties(path); });
}

TEST_F(IpiMetaDataTests, ValuesOfEachDataFile) {
	eachDataFile([this](const string &path) { checkValues(path); });
}

/**
 * Reading past the last property or map, or for a name the data file does
 * not have, gives no answer rather than one of another property.
 */
TEST_F(IpiMetaDataTests, NoPropertyPastTheEnd) {
	eachDataFile([](const string &path) {
		const auto manager = openIpiMetaData(path, nullptr);
		IpiMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
		const uint32_t count = fiftyoneDegreesIpiGetPropertyCount(dataSet);
		{
			EXCEPTION_CREATE;
			EXPECT_FALSE(fiftyoneDegreesIpiGetProperty(
				dataSet, count, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
		{
			EXCEPTION_CREATE;
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesIpiGetPropertyText(
				dataSet,
				count,
				FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
				buffer,
				sizeof(buffer),
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
		{
			EXCEPTION_CREATE;
			uint32_t mapCount = 0;
			ASSERT_TRUE(fiftyoneDegreesIpiGetProperty(
				dataSet, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, &mapCount, nullptr, nullptr,
				exception));
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesIpiGetPropertyMapText(
				dataSet,
				0,
				mapCount,
				buffer,
				sizeof(buffer),
				exception));
			EXPECT_TRUE(EXCEPTION_CHECK(
				FIFTYONE_DEGREES_STATUS_COLLECTION_INDEX_OUT_OF_RANGE));
		}
		{
			EXCEPTION_CREATE;
			EXPECT_EQ(-1, fiftyoneDegreesIpiGetPropertyIndex(
				dataSet,
				ipiMetaDataNoName,
				exception));
			EXPECT_TRUE(EXCEPTION_OKAY);
		}
	});
}

/**
 * Reading past the last value, or for a text a value does not have, gives
 * no answer rather than another value.
 */
TEST_F(IpiMetaDataTests, NoValuePastTheEnd) {
	eachDataFile([](const string &path) {
		const auto manager = openIpiMetaData(path, nullptr);
		IpiMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
		{
			EXCEPTION_CREATE;
			const uint32_t values =
				fiftyoneDegreesCollectionGetCount(dataSet->values);
			uint32_t propertyIndex = 0;
			EXPECT_FALSE(fiftyoneDegreesIpiGetValue(
				dataSet,
				values,
				&propertyIndex,
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
		{
			EXCEPTION_CREATE;
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesIpiGetValueText(
				dataSet,
				0,
				(fiftyoneDegreesValueText)99,
				buffer,
				sizeof(buffer),
				exception));
			EXPECT_TRUE(EXCEPTION_CHECK(FIFTYONE_DEGREES_STATUS_INVALID_INPUT));
		}
	});
}

/**
 * A property's values have no value of a name the property does not have,
 * nor a value of another property.
 */
TEST_F(IpiMetaDataTests, ValueNotOfTheProperty) {
	eachDataFile([](const string &path) {
		const auto manager = openIpiMetaData(path, nullptr);
		Ipi::MetaDataIpi metaData(manager);
		unique_ptr<Properties> properties(metaData.getProperties());
		vector<unique_ptr<Common::PropertyMetaData>> withValues;
		for (uint32_t i = 0;
			i < properties->getSize() && withValues.size() < 2;
			i++) {
			unique_ptr<Common::PropertyMetaData> property(
				properties->getByIndex(i));
			unique_ptr<Values> values(
				metaData.getValuesForProperty(property.get()));
			if (values->getSize() > 0) {
				withValues.push_back(std::move(property));
			}
		}
		ASSERT_EQ(2u, withValues.size());
		unique_ptr<Values> first(
			metaData.getValuesForProperty(withValues[0].get()));
		unique_ptr<Values> second(
			metaData.getValuesForProperty(withValues[1].get()));
		unique_ptr<Common::ValueMetaData> secondValue(second->getByIndex(0));
		ASSERT_TRUE(secondValue != nullptr);
		unique_ptr<Common::ValueMetaData> none(first->getByKey(
			Common::ValueMetaDataKey(
				withValues[0]->getName(),
				ipiMetaDataNoName)));
		EXPECT_TRUE(none == nullptr);
		unique_ptr<Common::ValueMetaData> other(first->getByKey(
			secondValue->getKey()));
		EXPECT_TRUE(other == nullptr);
		unique_ptr<Common::ValueMetaData> unknown(first->getByKey(
			Common::ValueMetaDataKey(ipiMetaDataNoName, "0")));
		EXPECT_TRUE(unknown == nullptr);
	});
}

/**
 * A data set created with some properties named gives those as available,
 * with their places among the names as their required property indexes, and
 * gives as available only the properties it holds.
 */
TEST_F(IpiMetaDataTests, AvailableProperties) {
	eachDataFile([](const string &path) {
		// The first two properties are named, or only the first when there
		// are two, so that one property is always left out.
		vector<string> names;
		{
			const auto all = openIpiMetaData(path, nullptr);
			IpiMetaDataDataSet scope(all.get());
			const uint32_t count =
				fiftyoneDegreesIpiGetPropertyCount(scope.dataSet);
			ASSERT_GE(count, 2u);
			for (uint32_t i = 0; i < 2 && i < count - 1; i++) {
				names.push_back(ipiMetaDataPropertyText(
					scope.dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
					ipiMetaDataTextBytes));
			}
		}
		string required = names[0];
		for (size_t n = 1; n < names.size(); n++) {
			required += "," + names[n];
		}
		const auto manager = openIpiMetaData(path, required.c_str());
		IpiMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetIpi *dataSet = scope.dataSet;
		uint32_t available = 0;
		const uint32_t count = fiftyoneDegreesIpiGetPropertyCount(dataSet);
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			bool isAvailable = false;
			int requiredIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesIpiGetProperty(
				dataSet, i, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, nullptr, &isAvailable,
				&requiredIndex, exception));
			const string name = ipiMetaDataPropertyText(
				dataSet,
				i,
				FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
				ipiMetaDataTextBytes);
			EXPECT_EQ(
				fiftyoneDegreesPropertiesGetPropertyIndexFromName(
					dataSet->b.b.available,
					name.c_str()) >= 0,
				isAvailable) << name;
			for (int n = 0; n < (int)names.size(); n++) {
				if (name == names[(size_t)n]) {
					EXPECT_TRUE(isAvailable) << name;
					EXPECT_EQ(n, requiredIndex) << name;
				}
			}
			if (isAvailable == false) {
				EXPECT_EQ(-1, requiredIndex) << name;
			}
			available += isAvailable ? 1 : 0;
		}
		EXPECT_EQ(dataSet->b.b.available->count, available);
		EXPECT_LT(available, count);
	});
}
