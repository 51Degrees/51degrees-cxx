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
 * @file HashMetaDataTests.cpp
 * @brief Checks the metadata the Hash C functions give for every property,
 * value, component and profile of each data file found against the data
 * set's own records, and that the C++ metadata gives the same answers.
 *
 * Each text is read twice, into a buffer that holds it and into one too
 * small, so both the written and the allocated text are checked.
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
using Components = Common::Collection<::byte, Common::ComponentMetaData>;
using Profiles = Common::Collection<uint32_t, Common::ProfileMetaData>;

/** Bytes that hold most texts, so the C functions write them in place. */
static const size_t hashMetaDataTextBytes = 1024;

/** Bytes too few for most texts, so the C functions allocate the text. */
static const size_t hashMetaDataShortBytes = 2;

/** Every value of a property with at most this many is checked. */
static const uint32_t hashMetaDataAllValues = 2000;

/** The values of a larger property checked, spread across them. */
static const uint32_t hashMetaDataSampledValues = 500;

/** The profiles checked, spread across them. */
static const uint32_t hashMetaDataSampledProfiles = 2000;

/** A name no property or value has. */
static const char *hashMetaDataNoName = "NoSuchName51Degrees";

/**
 * A manager over every property of a data file, with the least memory so the
 * collections read the file, which the metadata shares and which is freed
 * with it.
 */
static std::shared_ptr<fiftyoneDegreesResourceManager> openHashMetaData(
	const string &path,
	const char *requiredProperties) {
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = requiredProperties;
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
class HashMetaDataDataSet {
public:
	explicit HashMetaDataDataSet(fiftyoneDegreesResourceManager *manager)
		: dataSet(fiftyoneDegreesDataSetHashGet(manager)) {}
	~HashMetaDataDataSet() {
		fiftyoneDegreesDataSetHashRelease(dataSet);
	}
	fiftyoneDegreesDataSetHash *dataSet;
};

/**
 * A text a C function gave, freed when it is not the buffer. The exception
 * must not be set when the text is not NULL.
 */
static string hashMetaDataTake(
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
static string hashMetaDataStored(
	fiftyoneDegreesDataSetHash *dataSet,
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

/** A copy of the property at the index, as stored. */
static fiftyoneDegreesProperty hashMetaDataStoredProperty(
	fiftyoneDegreesDataSetHash *dataSet,
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
static fiftyoneDegreesValue hashMetaDataStoredValue(
	fiftyoneDegreesDataSetHash *dataSet,
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
static string hashMetaDataPropertyText(
	fiftyoneDegreesDataSetHash *dataSet,
	uint32_t index,
	fiftyoneDegreesPropertyText text,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesHashGetPropertyText(
		dataSet,
		index,
		text,
		buffer.data(),
		buffer.size(),
		exception);
	return hashMetaDataTake(result, buffer.data(), exception);
}

/** The name of a map of the property, read with a buffer of the length. */
static string hashMetaDataMapText(
	fiftyoneDegreesDataSetHash *dataSet,
	uint32_t index,
	uint32_t mapIndex,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesHashGetPropertyMapText(
		dataSet,
		index,
		mapIndex,
		buffer.data(),
		buffer.size(),
		exception);
	return hashMetaDataTake(result, buffer.data(), exception);
}

/** A text of the value, read with a buffer of the length given. */
static string hashMetaDataValueText(
	fiftyoneDegreesDataSetHash *dataSet,
	uint32_t index,
	fiftyoneDegreesValueText text,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesHashGetValueText(
		dataSet,
		index,
		text,
		buffer.data(),
		buffer.size(),
		exception);
	return hashMetaDataTake(result, buffer.data(), exception);
}

/** The name of the component, read with a buffer of the length given. */
static string hashMetaDataComponentName(
	fiftyoneDegreesDataSetHash *dataSet,
	uint32_t index,
	size_t length) {
	EXCEPTION_CREATE;
	vector<char> buffer(length);
	char *result = fiftyoneDegreesHashGetComponentName(
		dataSet,
		index,
		buffer.data(),
		buffer.size(),
		exception);
	return hashMetaDataTake(result, buffer.data(), exception);
}

/** The default value text the C library writes for the property. */
static string hashMetaDataDefaultText(
	fiftyoneDegreesDataSetHash *dataSet,
	const fiftyoneDegreesProperty &property) {
	EXCEPTION_CREATE;
	vector<char> buffer(hashMetaDataTextBytes);
	// Hash data sets have no property types.
	char *result = fiftyoneDegreesValueGetDefaultText(
		dataSet->values,
		dataSet->strings,
		nullptr,
		&property,
		buffer.data(),
		buffer.size(),
		exception);
	return hashMetaDataTake(result, buffer.data(), exception);
}

class HashMetaDataTests : public Base {
protected:
	/** Runs the check on each data file found, or skips the test. */
	template <typename Check>
	void eachDataFile(Check check) {
		bool any = false;
		for (int i = 0; i < _HashFileNamesLength; i++) {
			const string path = GetFilePath(
				_dataFolderName,
				_HashFileNames[i]);
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
			GTEST_SKIP() << "No Device Detection data file was found.";
		}
	}

	/** Checks every property's fields, texts and maps. */
	void checkProperties(const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		Hash::MetaDataHash metaData(manager);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		unique_ptr<Properties> properties(metaData.getProperties());
		const uint32_t count = fiftyoneDegreesHashGetPropertyCount(dataSet);
		EXPECT_EQ(
			fiftyoneDegreesCollectionGetCount(dataSet->properties),
			count);
		EXPECT_EQ(count, properties->getSize());
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			const fiftyoneDegreesProperty stored =
				hashMetaDataStoredProperty(dataSet, i);
			const string name = hashMetaDataStored(
				dataSet,
				stored.nameOffset);

			uint8_t valueType = 0, componentId = 0, displayOrder = 0;
			bool isMandatory = false, isList = false, isObsolete = false;
			bool show = false, showValues = false, isAvailable = false;
			uint32_t mapCount = 0;
			int requiredIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesHashGetProperty(
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
			const int found = fiftyoneDegreesHashGetPropertyIndex(
				dataSet,
				name.c_str(),
				exception);
			EXPECT_TRUE(EXCEPTION_OKAY) << name;
			ASSERT_GE(found, 0) << name;
			EXPECT_LE((uint32_t)found, i) << name;
			EXPECT_EQ(name, hashMetaDataStored(
				dataSet,
				hashMetaDataStoredProperty(
					dataSet,
					(uint32_t)found).nameOffset)) << name;

			const string description =
				(int32_t)stored.descriptionOffset == -1
				? string()
				: hashMetaDataStored(dataSet, stored.descriptionOffset);
			const string category = (int32_t)stored.categoryOffset == -1
				? string()
				: hashMetaDataStored(dataSet, stored.categoryOffset);
			const string url = (int32_t)stored.urlOffset <= 0
				? string()
				: hashMetaDataStored(dataSet, stored.urlOffset);
			const string defaultValue =
				hashMetaDataDefaultText(dataSet, stored);
			for (size_t length : {
				hashMetaDataTextBytes,
				hashMetaDataShortBytes }) {
				EXPECT_EQ(name, hashMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
					length)) << name;
				EXPECT_EQ(description, hashMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_DESCRIPTION,
					length)) << name;
				EXPECT_EQ(category, hashMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_CATEGORY,
					length)) << name;
				EXPECT_EQ(url, hashMetaDataPropertyText(
					dataSet,
					i,
					FIFTYONE_DEGREES_PROPERTY_TEXT_URL,
					length)) << name;
				EXPECT_EQ(defaultValue, hashMetaDataPropertyText(
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
				const string map = hashMetaDataStored(dataSet, offset);
				maps.push_back(map);
				EXPECT_EQ(map, hashMetaDataMapText(
					dataSet,
					i,
					m,
					hashMetaDataTextBytes)) << name;
				EXPECT_EQ(map, hashMetaDataMapText(
					dataSet,
					i,
					m,
					hashMetaDataShortBytes)) << name;
			}

			uint32_t firstValueIndex = 0, valueCount = 0;
			long defaultValueIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesHashGetPropertyValueIndexes(
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
			// Hash data sets have no property types.
			EXPECT_EQ(
				fiftyoneDegreesValueGetDefaultIndex(
					dataSet->values,
					dataSet->strings,
					nullptr,
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
		const auto manager = openHashMetaData(path, nullptr);
		Hash::MetaDataHash metaData(manager);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		unique_ptr<Properties> properties(metaData.getProperties());
		const uint32_t count = fiftyoneDegreesHashGetPropertyCount(dataSet);
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			uint32_t first = 0, valueCount = 0;
			ASSERT_TRUE(fiftyoneDegreesHashGetPropertyValueIndexes(
				dataSet,
				i,
				&first,
				&valueCount,
				nullptr,
				exception));
			unique_ptr<Common::PropertyMetaData> property(
				properties->getByIndex(i));
			const string &name = property->getName();
			unique_ptr<Values> values(
				metaData.getValuesForProperty(property.get()));
			EXPECT_EQ(valueCount, values->getSize()) << name;
			const uint32_t step = valueCount <= hashMetaDataAllValues
				? 1
				: valueCount / hashMetaDataSampledValues;
			for (uint32_t v = 0; v < valueCount; v += step) {
				const uint32_t valueIndex = first + v;
				const fiftyoneDegreesValue stored =
					hashMetaDataStoredValue(dataSet, valueIndex);
				uint32_t propertyIndex = UINT32_MAX;
				ASSERT_TRUE(fiftyoneDegreesHashGetValue(
					dataSet,
					valueIndex,
					&propertyIndex,
					exception)) << name;
				EXPECT_EQ((uint32_t)stored.propertyIndex, propertyIndex);
				EXPECT_EQ(i, propertyIndex) << name;
				const string valueName = hashMetaDataStored(
					dataSet,
					(uint32_t)stored.nameOffset);
				const string description = stored.descriptionOffset <= 0
					? string()
					: hashMetaDataStored(
						dataSet,
						(uint32_t)stored.descriptionOffset);
				const string url = stored.urlOffsetOrWeight <= 0
					? string()
					: hashMetaDataStored(
						dataSet,
						(uint32_t)stored.urlOffsetOrWeight);
				for (size_t length : {
					hashMetaDataTextBytes,
					hashMetaDataShortBytes }) {
					EXPECT_EQ(valueName, hashMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_NAME,
						length)) << name;
					EXPECT_EQ(description, hashMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_DESCRIPTION,
						length)) << name;
					EXPECT_EQ(url, hashMetaDataValueText(
						dataSet,
						valueIndex,
						FIFTYONE_DEGREES_VALUE_TEXT_URL,
						length)) << name;
				}
				EXPECT_EQ(
					(long)valueIndex,
					fiftyoneDegreesHashGetValueIndex(
						dataSet,
						i,
						valueName.c_str(),
						exception)) << name << " " << valueName;
				EXPECT_TRUE(EXCEPTION_OKAY) << name;

				// The C++ metadata gives the same answers.
				unique_ptr<Common::ValueMetaData> value(
					values->getByIndex(v));
				ASSERT_TRUE(value != nullptr) << name;
				EXPECT_EQ(name, value->getKey().getPropertyName());
				EXPECT_EQ(valueName, value->getName()) << name;
				EXPECT_EQ(description, value->getDescription()) << name;
				EXPECT_EQ(url, value->getUrl()) << name;
				unique_ptr<Common::ValueMetaData> byKey(
					values->getByKey(value->getKey()));
				ASSERT_TRUE(byKey != nullptr) << name << " " << valueName;
				EXPECT_EQ(valueName, byKey->getName()) << name;
			}
		}
	}

	/** Checks the components and the profiles. */
	void checkComponentsAndProfiles(const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		Hash::MetaDataHash metaData(manager);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		unique_ptr<Components> components(metaData.getComponents());
		const uint32_t count = fiftyoneDegreesHashGetComponentCount(dataSet);
		EXPECT_EQ(dataSet->componentsList.count, count);
		EXPECT_EQ(count, components->getSize());
		for (uint32_t c = 0; c < count; c++) {
			EXCEPTION_CREATE;
			fiftyoneDegreesComponent *stored = (fiftyoneDegreesComponent*)
				dataSet->componentsList.items[c].data.ptr;
			uint8_t componentId = 0;
			uint32_t defaultProfileId = 0;
			ASSERT_TRUE(fiftyoneDegreesHashGetComponent(
				dataSet,
				c,
				&componentId,
				&defaultProfileId,
				exception));
			EXPECT_EQ(stored->componentId, componentId);
			EXPECT_EQ(
				fiftyoneDegreesComponentGetDefaultProfileId(
					dataSet->profiles,
					stored,
					exception),
				defaultProfileId);
			const string name = hashMetaDataStored(
				dataSet,
				(uint32_t)stored->nameOffset);
			EXPECT_EQ(name, hashMetaDataComponentName(
				dataSet,
				c,
				hashMetaDataTextBytes));
			EXPECT_EQ(name, hashMetaDataComponentName(
				dataSet,
				c,
				hashMetaDataShortBytes));
			unique_ptr<Common::ComponentMetaData> component(
				components->getByIndex(c));
			ASSERT_TRUE(component != nullptr) << name;
			EXPECT_EQ(componentId, component->getComponentId()) << name;
			EXPECT_EQ(name, component->getName());
			EXPECT_EQ(defaultProfileId, component->getDefaultProfileId());
			unique_ptr<Common::ComponentMetaData> byKey(
				components->getByKey(componentId));
			ASSERT_TRUE(byKey != nullptr) << name;
			EXPECT_EQ(name, byKey->getName());
		}

		unique_ptr<Profiles> profiles(metaData.getProfiles());
		const uint32_t profileCount =
			fiftyoneDegreesHashGetProfileCount(dataSet);
		EXPECT_EQ(
			fiftyoneDegreesCollectionGetCount(dataSet->profileOffsets),
			profileCount);
		EXPECT_EQ(profileCount, profiles->getSize());
		const uint32_t step =
			profileCount / hashMetaDataSampledProfiles + 1;
		for (uint32_t p = 0; p < profileCount; p += step) {
			EXCEPTION_CREATE;
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			const fiftyoneDegreesProfile *stored =
				fiftyoneDegreesProfileGetByIndex(
					dataSet->profileOffsets,
					dataSet->profiles,
					p,
					&item,
					exception);
			EXCEPTION_THROW;
			const uint32_t storedId = stored->profileId;
			const uint8_t storedComponentId =
				((fiftyoneDegreesComponent*)dataSet->componentsList.items[
					stored->componentIndex].data.ptr)->componentId;
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->profiles, &item);
			uint32_t profileId = 0;
			uint8_t componentId = 0;
			ASSERT_TRUE(fiftyoneDegreesHashGetProfile(
				dataSet,
				p,
				&profileId,
				&componentId,
				exception));
			EXPECT_EQ(storedId, profileId);
			EXPECT_EQ(storedComponentId, componentId);
			uint8_t byIdComponentId = 0;
			ASSERT_TRUE(fiftyoneDegreesHashGetProfileById(
				dataSet,
				profileId,
				&byIdComponentId,
				exception));
			EXPECT_EQ(storedComponentId, byIdComponentId);
			unique_ptr<Common::ProfileMetaData> profile(
				profiles->getByIndex(p));
			ASSERT_TRUE(profile != nullptr);
			EXPECT_EQ(profileId, profile->getProfileId());
			EXPECT_EQ(componentId, profile->getComponentId());
			unique_ptr<Common::ProfileMetaData> byKey(
				profiles->getByKey(profileId));
			ASSERT_TRUE(byKey != nullptr);
			EXPECT_EQ(componentId, byKey->getComponentId());
		}
	}
};

TEST_F(HashMetaDataTests, PropertiesOfEachDataFile) {
	eachDataFile([this](const string &path) { checkProperties(path); });
}

TEST_F(HashMetaDataTests, ValuesOfEachDataFile) {
	eachDataFile([this](const string &path) { checkValues(path); });
}

TEST_F(HashMetaDataTests, ComponentsAndProfilesOfEachDataFile) {
	eachDataFile([this](const string &path) {
		checkComponentsAndProfiles(path);
	});
}

/**
 * Reading past the last property or map, or for a name the data file does
 * not have, gives no answer rather than one of another property.
 */
TEST_F(HashMetaDataTests, NoPropertyPastTheEnd) {
	eachDataFile([](const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		const uint32_t count = fiftyoneDegreesHashGetPropertyCount(dataSet);
		{
			EXCEPTION_CREATE;
			EXPECT_FALSE(fiftyoneDegreesHashGetProperty(
				dataSet, count, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
		{
			EXCEPTION_CREATE;
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesHashGetPropertyText(
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
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesHashGetPropertyText(
				dataSet,
				0,
				(fiftyoneDegreesPropertyText)99,
				buffer,
				sizeof(buffer),
				exception));
			EXPECT_TRUE(EXCEPTION_CHECK(FIFTYONE_DEGREES_STATUS_INVALID_INPUT));
		}
		{
			EXCEPTION_CREATE;
			uint32_t mapCount = 0;
			ASSERT_TRUE(fiftyoneDegreesHashGetProperty(
				dataSet, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, &mapCount, nullptr, nullptr,
				exception));
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesHashGetPropertyMapText(
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
			EXPECT_EQ(-1, fiftyoneDegreesHashGetPropertyIndex(
				dataSet,
				hashMetaDataNoName,
				exception));
			EXPECT_TRUE(EXCEPTION_OKAY);
		}
	});
}

/**
 * Reading past the last value, or for a name the property does not have,
 * gives no answer rather than another value.
 */
TEST_F(HashMetaDataTests, NoValuePastTheEnd) {
	eachDataFile([](const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		{
			EXCEPTION_CREATE;
			const int isMobile = fiftyoneDegreesHashGetPropertyIndex(
				dataSet,
				"IsMobile",
				exception);
			ASSERT_GE(isMobile, 0);
			EXPECT_EQ(-1, fiftyoneDegreesHashGetValueIndex(
				dataSet,
				(uint32_t)isMobile,
				hashMetaDataNoName,
				exception));
			EXPECT_TRUE(EXCEPTION_OKAY);
		}
		{
			EXCEPTION_CREATE;
			const uint32_t values =
				fiftyoneDegreesCollectionGetCount(dataSet->values);
			uint32_t propertyIndex = 0;
			EXPECT_FALSE(fiftyoneDegreesHashGetValue(
				dataSet,
				values,
				&propertyIndex,
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
	});
}

/**
 * Reading past the last component, or for a profile id the data file does
 * not have, gives no answer rather than another component or profile.
 */
TEST_F(HashMetaDataTests, NoComponentPastTheEnd) {
	eachDataFile([](const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		{
			EXCEPTION_CREATE;
			const uint32_t components =
				fiftyoneDegreesHashGetComponentCount(dataSet);
			uint8_t componentId = 0;
			uint32_t defaultProfileId = 0;
			EXPECT_FALSE(fiftyoneDegreesHashGetComponent(
				dataSet,
				components,
				&componentId,
				&defaultProfileId,
				exception));
			EXPECT_TRUE(EXCEPTION_OKAY);
			char buffer[8];
			EXPECT_EQ(nullptr, fiftyoneDegreesHashGetComponentName(
				dataSet,
				components,
				buffer,
				sizeof(buffer),
				exception));
			EXPECT_TRUE(EXCEPTION_CHECK(
				FIFTYONE_DEGREES_STATUS_COLLECTION_INDEX_OUT_OF_RANGE));
		}
		{
			EXCEPTION_CREATE;
			uint8_t componentId = 0;
			// No profile has the id 0, which a device id gives for a
			// component with no profile.
			EXPECT_FALSE(fiftyoneDegreesHashGetProfileById(
				dataSet,
				0,
				&componentId,
				exception));
			EXPECT_FALSE(EXCEPTION_OKAY);
		}
	});
}

/**
 * A property's values have no value of a name the property does not have,
 * nor a value of another property, which the C++ collection read through a
 * null pointer for.
 */
TEST_F(HashMetaDataTests, ValueNotOfTheProperty) {
	eachDataFile([](const string &path) {
		const auto manager = openHashMetaData(path, nullptr);
		Hash::MetaDataHash metaData(manager);
		unique_ptr<Properties> properties(metaData.getProperties());
		unique_ptr<Common::PropertyMetaData> isMobile(
			properties->getByKey("IsMobile"));
		ASSERT_TRUE(isMobile != nullptr);
		unique_ptr<Common::PropertyMetaData> browser(
			properties->getByKey("BrowserName"));
		ASSERT_TRUE(browser != nullptr);
		unique_ptr<Values> isMobileValues(
			metaData.getValuesForProperty(isMobile.get()));
		unique_ptr<Values> browserValues(
			metaData.getValuesForProperty(browser.get()));
		ASSERT_GT(browserValues->getSize(), 0u);
		unique_ptr<Common::ValueMetaData> browserValue(
			browserValues->getByIndex(0));
		ASSERT_TRUE(browserValue != nullptr);

		unique_ptr<Common::ValueMetaData> none(isMobileValues->getByKey(
			Common::ValueMetaDataKey("IsMobile", hashMetaDataNoName)));
		EXPECT_TRUE(none == nullptr);
		unique_ptr<Common::ValueMetaData> other(isMobileValues->getByKey(
			browserValue->getKey()));
		EXPECT_TRUE(other == nullptr);
		unique_ptr<Common::ValueMetaData> unknown(isMobileValues->getByKey(
			Common::ValueMetaDataKey(hashMetaDataNoName, "True")));
		EXPECT_TRUE(unknown == nullptr);
		unique_ptr<Common::ValueMetaData> own(isMobileValues->getByKey(
			Common::ValueMetaDataKey("IsMobile", "True")));
		ASSERT_TRUE(own != nullptr);
		EXPECT_EQ("True", own->getName());
		unique_ptr<Common::ValueMetaData> mine(browserValues->getByKey(
			browserValue->getKey()));
		ASSERT_TRUE(mine != nullptr);
		EXPECT_EQ(browserValue->getName(), mine->getName());

		unique_ptr<Values> all(metaData.getValues());
		unique_ptr<Common::ValueMetaData> nowhere(all->getByKey(
			Common::ValueMetaDataKey("IsMobile", hashMetaDataNoName)));
		EXPECT_TRUE(nowhere == nullptr);
	});
}

/**
 * A data set created with some properties named gives those as available,
 * with their places among the names as their required property indexes, and
 * gives as available only the properties it holds.
 */
TEST_F(HashMetaDataTests, AvailableProperties) {
	eachDataFile([](const string &path) {
		const auto manager = openHashMetaData(path, "IsMobile,BrowserName");
		HashMetaDataDataSet scope(manager.get());
		fiftyoneDegreesDataSetHash *dataSet = scope.dataSet;
		uint32_t available = 0;
		const uint32_t count = fiftyoneDegreesHashGetPropertyCount(dataSet);
		for (uint32_t i = 0; i < count; i++) {
			EXCEPTION_CREATE;
			bool isAvailable = false;
			int requiredIndex = -2;
			ASSERT_TRUE(fiftyoneDegreesHashGetProperty(
				dataSet, i, nullptr, nullptr, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr, nullptr, &isAvailable,
				&requiredIndex, exception));
			const string name = hashMetaDataPropertyText(
				dataSet,
				i,
				FIFTYONE_DEGREES_PROPERTY_TEXT_NAME,
				hashMetaDataTextBytes);
			EXPECT_EQ(
				fiftyoneDegreesPropertiesGetPropertyIndexFromName(
					dataSet->b.b.available,
					name.c_str()) >= 0,
				isAvailable) << name;
			if (name == "IsMobile") {
				EXPECT_TRUE(isAvailable);
				EXPECT_EQ(0, requiredIndex);
			}
			if (name == "BrowserName") {
				EXPECT_TRUE(isAvailable);
				EXPECT_EQ(1, requiredIndex);
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