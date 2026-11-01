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
 * @file HashValueProfilesTests.cpp
 * @brief Tests the index of the profiles holding each value of the properties
 * a caller names, through the init functions that take the names,
 * fiftyoneDegreesHashGetProfileIdsForPropertyAndValue,
 * fiftyoneDegreesHashIterateProfilesForPropertyAndValue,
 * ConfigHash::setIndexedProperties and
 * EngineHash::getProfileIdsForPropertyValue.
 *
 * Every answer is compared with the profiles each value really has, found by
 * reading the values of every profile, and with a data set that indexes
 * nothing and so reads every profile for each lookup. The tests use the Lite
 * data file, so they run in every build. The Lite file has no NativeModel or
 * TAC, so other properties are indexed instead, and a copy of the file in
 * which one profile holds two values of a property stands in for a list.
 * NativeModel and TAC are checked as well when an Enterprise data file is
 * present.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/hash.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

/** Ids of profiles, in the order a lookup gives them. */
typedef std::vector<uint32_t> ProfileIds;

/** The ids of the profiles holding each value, by index of the value. */
typedef std::map<uint32_t, ProfileIds> ValueProfileIds;

/** Names of the properties to index, as the C functions take them. */
typedef std::vector<const char*> Names;

/** A property with the names of its values, in the order of their indexes. */
struct PropertyValues {
	uint32_t firstValueIndex = 0;
	uint8_t componentIndex = 0;
	std::vector<std::string> names;
};

/** A copy of the data file in which one profile holds a list of values. */
struct ListData {
	std::vector<unsigned char> data;
	uint32_t profileId = 0; /* The profile changed */
	uint32_t kept = 0; /* Index of the value of the first property it kept */
	uint32_t added = 0; /* Index of the value of the first property added */
	uint32_t removed = 0; /* Index of the value of the second it lost */
};

/** Every performance profile of the C API. */
const struct {
	const char *name;
	const fiftyoneDegreesConfigHash *config;
} performanceProfiles[] = {
	{ "InMemory", &fiftyoneDegreesHashInMemoryConfig },
	{ "HighPerformance", &fiftyoneDegreesHashHighPerformanceConfig },
	{ "LowMemory", &fiftyoneDegreesHashLowMemoryConfig },
	{ "Balanced", &fiftyoneDegreesHashBalancedConfig },
	{ "BalancedTemp", &fiftyoneDegreesHashBalancedTempConfig },
	{ "Default", &fiftyoneDegreesHashDefaultConfig },
};

/** Frees the manager when it goes out of scope, a failed test included. */
class Manager {
public:
	fiftyoneDegreesResourceManager manager;
	bool initialised = false;
	~Manager() {
		if (initialised) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
	}
	fiftyoneDegreesResourceManager* get() { return &manager; }
};

/** An engine that can say whether its data set has the index. */
class IndexedEngine : public EngineHash {
public:
	using EngineHash::EngineHash;
	bool hasIndex() const {
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(manager.get());
		const bool has = dataSet->valueProfiles != nullptr;
		fiftyoneDegreesDataSetHashRelease(dataSet);
		return has;
	}
};

/** Initialises the manager from the file, indexing the properties named. */
void initFromFile(
	Manager &manager,
	fiftyoneDegreesConfigHash config,
	const char *required,
	const std::string &path,
	Names indexed = Names()) {
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = required;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesHashInitManagerFromFileWithIndexedProperties(
			manager.get(),
			&config,
			&properties,
			path.c_str(),
			indexed.empty() ? nullptr : indexed.data(),
			(int)indexed.size(),
			exception));
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	manager.initialised = true;
}

/**
 * Initialises the manager from memory that stays with the caller, indexing
 * the properties named.
 */
void initFromMemory(
	Manager &manager,
	fiftyoneDegreesConfigHash config,
	const char *required,
	std::vector<unsigned char> &data,
	Names indexed = Names()) {
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = required;
	config.b.b.freeData = false;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesHashInitManagerFromMemoryWithIndexedProperties(
			manager.get(),
			&config,
			&properties,
			data.data(),
			(long)data.size(),
			indexed.empty() ? nullptr : indexed.data(),
			(int)indexed.size(),
			exception));
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	manager.initialised = true;
}

void readFile(const std::string &path, std::vector<unsigned char> &data) {
	FILE *file = fopen(path.c_str(), "rb");
	ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
	fseek(file, 0, SEEK_END);
	data.resize((size_t)ftell(file));
	rewind(file);
	const size_t bytes = fread(data.data(), 1, data.size(), file);
	fclose(file);
	ASSERT_EQ(data.size(), bytes) << "Could not read '" << path << "'.";
}

/**
 * Finds the ids of the profiles holding each value from first to last by
 * reading the values of every profile, in the order of the profile offsets.
 */
ValueProfileIds readValueProfileIds(
	Manager &manager,
	uint32_t first = 0,
	uint32_t last = UINT32_MAX) {
	ValueProfileIds found;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesDataSetHash *dataSet =
		fiftyoneDegreesDataSetHashGet(manager.get());
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const uint32_t count =
		fiftyoneDegreesCollectionGetCount(dataSet->profileOffsets);
	for (uint32_t i = 0; i < count; i++) {
		fiftyoneDegreesProfile *profile = fiftyoneDegreesProfileGetByIndex(
			dataSet->profileOffsets,
			dataSet->profiles,
			i,
			&item,
			exception);
		if (profile == nullptr) {
			ADD_FAILURE() << "Could not read the profile at " << i << ".";
			break;
		}
		const uint32_t *values = (const uint32_t*)(profile + 1);
		for (uint32_t v = 0; v < profile->valueCount; v++) {
			if (values[v] >= first && values[v] <= last) {
				found[values[v]].push_back(profile->profileId);
			}
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->profiles, &item);
	}
	fiftyoneDegreesDataSetHashRelease(dataSet);
	return found;
}

/** Gets the names of the values of the property. */
PropertyValues getValues(Manager &manager, const std::string &propertyName) {
	PropertyValues found;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesDataSetHash *dataSet =
		fiftyoneDegreesDataSetHashGet(manager.get());
	fiftyoneDegreesCollectionItem propertyItem, valueItem, nameItem;
	fiftyoneDegreesDataReset(&propertyItem.data);
	fiftyoneDegreesDataReset(&valueItem.data);
	fiftyoneDegreesDataReset(&nameItem.data);
	const fiftyoneDegreesProperty *property = fiftyoneDegreesPropertyGetByName(
		dataSet->properties,
		dataSet->strings,
		propertyName.c_str(),
		&propertyItem,
		exception);
	if (property != nullptr && FIFTYONE_DEGREES_EXCEPTION_OKAY) {
		found.firstValueIndex = property->firstValueIndex;
		found.componentIndex = property->componentIndex;
		for (uint64_t v = property->firstValueIndex;
			property->firstValueIndex <= property->lastValueIndex &&
			v <= property->lastValueIndex;
			v++) {
			const fiftyoneDegreesValue *value = fiftyoneDegreesValueGet(
				dataSet->values,
				(uint32_t)v,
				&valueItem,
				exception);
			if (value == nullptr) {
				ADD_FAILURE() << "Could not read the value at " << v << ".";
				break;
			}
			const fiftyoneDegreesString *name = fiftyoneDegreesValueGetName(
				dataSet->strings,
				value,
				&nameItem,
				exception);
			found.names.push_back(&name->value);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &nameItem);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &valueItem);
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &propertyItem);
	}
	fiftyoneDegreesDataSetHashRelease(dataSet);
	return found;
}

/**
 * Gets the names of the properties in the data file, in the order of the
 * properties collection, of the component or of every component when it is
 * negative.
 */
std::vector<std::string> getPropertyNames(
	Manager &manager,
	int componentIndex) {
	std::vector<std::string> names;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesDataSetHash *dataSet =
		fiftyoneDegreesDataSetHashGet(manager.get());
	fiftyoneDegreesCollectionItem propertyItem, nameItem;
	fiftyoneDegreesDataReset(&propertyItem.data);
	fiftyoneDegreesDataReset(&nameItem.data);
	const uint32_t count =
		fiftyoneDegreesCollectionGetCount(dataSet->properties);
	for (uint32_t i = 0; i < count; i++) {
		fiftyoneDegreesProperty *property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			i,
			&propertyItem,
			exception);
		if (property == nullptr) {
			ADD_FAILURE() << "Could not read the property at " << i << ".";
			break;
		}
		if (componentIndex < 0 || property->componentIndex == componentIndex) {
			const fiftyoneDegreesString *name = fiftyoneDegreesPropertyGetName(
				dataSet->strings,
				property,
				&nameItem,
				exception);
			names.push_back(&name->value);
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->strings, &nameItem);
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &propertyItem);
	}
	fiftyoneDegreesDataSetHashRelease(dataSet);
	return names;
}

/** Points to each of the names, which must outlive the pointers. */
Names pointTo(const std::vector<std::string> &names) {
	Names pointers;
	for (const std::string &name : names) {
		pointers.push_back(name.c_str());
	}
	return pointers;
}

/** Gets the ids with the function that returns them. */
ProfileIds getIds(
	Manager &manager,
	const std::string &propertyName,
	const std::string &valueName) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const uint32_t count = fiftyoneDegreesHashGetProfileIdsForPropertyAndValue(
		manager.get(),
		propertyName.c_str(),
		valueName.c_str(),
		nullptr,
		0,
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	ProfileIds ids(count);
	if (count > 0) {
		EXPECT_EQ(
			count,
			fiftyoneDegreesHashGetProfileIdsForPropertyAndValue(
				manager.get(),
				propertyName.c_str(),
				valueName.c_str(),
				ids.data(),
				count,
				exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	}
	return ids;
}

bool addId(void *state, fiftyoneDegreesCollectionItem *item) {
	((ProfileIds*)state)->push_back(
		((fiftyoneDegreesProfile*)item->data.ptr)->profileId);
	return true;
}

/** Gets the ids of the profiles the iterate function passes. */
ProfileIds iterateIds(
	Manager &manager,
	const std::string &propertyName,
	const std::string &valueName) {
	ProfileIds ids;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	const uint32_t count =
		fiftyoneDegreesHashIterateProfilesForPropertyAndValue(
			manager.get(),
			propertyName.c_str(),
			valueName.c_str(),
			&ids,
			addId,
			exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
	EXPECT_EQ((size_t)count, ids.size());
	return ids;
}

const fiftyoneDegreesHashValueProfiles* getIndex(Manager &manager) {
	fiftyoneDegreesDataSetHash *dataSet =
		fiftyoneDegreesDataSetHashGet(manager.get());
	const fiftyoneDegreesHashValueProfiles *index = dataSet->valueProfiles;
	fiftyoneDegreesDataSetHashRelease(dataSet);
	return index;
}

bool contains(const ProfileIds &ids, uint32_t id) {
	for (uint32_t i : ids) {
		if (i == id) {
			return true;
		}
	}
	return false;
}

std::string describe(const ProfileIds &ids) {
	std::stringstream text;
	text << ids.size() << " [";
	for (size_t i = 0; i < ids.size() && i < 8; i++) {
		text << (i > 0 ? " " : "") << ids[i];
	}
	text << (ids.size() > 8 ? " ...]" : "]");
	return text.str();
}

/**
 * Checks that both lookups give the profiles the value really has, setting a
 * description of the difference when there is one and none is set yet.
 */
bool lookupsMatch(
	Manager &manager,
	const std::string &propertyName,
	const std::string &valueName,
	const ProfileIds &expected,
	std::string &difference) {
	const ProfileIds got = getIds(manager, propertyName, valueName);
	const ProfileIds iterated = iterateIds(manager, propertyName, valueName);
	if (got == expected && iterated == expected) {
		return true;
	}
	if (difference.empty()) {
		difference = propertyName + " '" + valueName + "' should give " +
			describe(expected) + " but the ids were " + describe(got) +
			" and the iteration " + describe(iterated);
	}
	return false;
}

/**
 * Expects both lookups to give the profiles the value really has. The context
 * is added to a failure, as SCOPED_TRACE keeps memory that the memory check
 * of the base class would count.
 */
void expectLookups(
	Manager &manager,
	const std::string &propertyName,
	const std::string &valueName,
	const ProfileIds &expected,
	const std::string &context = std::string()) {
	std::string difference;
	EXPECT_TRUE(lookupsMatch(
		manager,
		propertyName,
		valueName,
		expected,
		difference)) << context << difference;
}

/**
 * Finds the first value of the property held by exactly the number of
 * profiles, or by more than the number when more is true. Returns the index
 * in the property's values, or -1 when there is none.
 */
int findValue(
	const PropertyValues &values,
	const ValueProfileIds &found,
	size_t profiles,
	bool more) {
	for (size_t i = 0; i < values.names.size(); i++) {
		const auto entry = found.find(values.firstValueIndex + (uint32_t)i);
		const size_t count = entry == found.end() ? 0 : entry->second.size();
		if (more ? count > profiles : count == profiles) {
			return (int)i;
		}
	}
	return -1;
}

ProfileIds expectedFor(const ValueProfileIds &found, uint32_t valueIndex) {
	const auto entry = found.find(valueIndex);
	return entry == found.end() ? ProfileIds() : entry->second;
}

/**
 * Makes a copy of the data in which one profile holds two values of the first
 * property and none of the second, as a profile holding a list does. Every
 * value of the second property must come after those of the first.
 */
void makeListData(
	std::vector<unsigned char> &original,
	const char *first,
	const char *second,
	ListData &list) {
	size_t position = 0;
	list.data = original;
	{
		Manager reader;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			reader,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			list.data));
		const PropertyValues a = getValues(reader, first);
		const PropertyValues b = getValues(reader, second);
		ASSERT_FALSE(a.names.empty()) << "The data file has no " << first;
		ASSERT_FALSE(b.names.empty()) << "The data file has no " << second;
		const uint32_t lastA =
			a.firstValueIndex + (uint32_t)a.names.size() - 1;
		const uint32_t lastB =
			b.firstValueIndex + (uint32_t)b.names.size() - 1;
		ASSERT_LT(lastA, b.firstValueIndex);

		// The profiles of a data set from memory are in that memory, so the
		// position of a profile's value in the copy is found from them.
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(reader.get());
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const uint32_t count =
			fiftyoneDegreesCollectionGetCount(dataSet->profileOffsets);
		for (uint32_t i = 0; i < count && list.profileId == 0; i++) {
			fiftyoneDegreesProfile *profile = fiftyoneDegreesProfileGetByIndex(
				dataSet->profileOffsets,
				dataSet->profiles,
				i,
				&item,
				exception);
			if (profile == nullptr) {
				break;
			}
			const uint32_t *values = (const uint32_t*)(profile + 1);
			for (uint32_t v = 0; v + 1 < profile->valueCount; v++) {
				if (values[v] >= a.firstValueIndex && values[v] < lastA &&
					values[v + 1] >= b.firstValueIndex &&
					values[v + 1] <= lastB) {
					list.profileId = profile->profileId;
					list.kept = values[v];
					list.added = values[v] + 1;
					list.removed = values[v + 1];
					position = (size_t)(
						(const unsigned char*)&values[v + 1] - list.data.data());
					break;
				}
			}
			FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->profiles, &item);
		}
		fiftyoneDegreesDataSetHashRelease(dataSet);
	}
	ASSERT_NE(0u, list.profileId) <<
		"No profile holds a value of both " << first << " and " << second;
	ASSERT_LE(position + sizeof(uint32_t), list.data.size());
	memcpy(list.data.data() + position, &list.added, sizeof(uint32_t));
}

}

class HashValueProfilesTests : public Base {
protected:
	std::string path;

	void SetUp() override {
		Base::SetUp();
		path = GetFilePath(_dataFolderName, "51Degrees-LiteV4.1.hash");
		ASSERT_FALSE(path.empty()) << "The Lite data file was not found.";
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(path);
		Base::TearDown();
	}
};

/** A value one profile holds gives that profile alone. */
TEST_F(HashValueProfilesTests, OneProfile) {
	Manager manager;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "BrowserVersion" }));
	const fiftyoneDegreesHashValueProfiles *index = getIndex(manager);
	ASSERT_NE(nullptr, index);
	ASSERT_EQ(1u, index->propertiesCount);
	EXPECT_STREQ("BrowserVersion", index->names[0]);
	EXPECT_NE(UINT32_MAX, index->propertyIndexes[0]);
	const ValueProfileIds found = readValueProfileIds(manager);
	const PropertyValues values = getValues(manager, "BrowserVersion");
	const int i = findValue(values, found, 1, false);
	ASSERT_GE(i, 0) << "No BrowserVersion is held by exactly one profile.";
	const ProfileIds expected =
		expectedFor(found, values.firstValueIndex + (uint32_t)i);
	ASSERT_EQ(1u, expected.size());
	expectLookups(manager, "BrowserVersion", values.names[i], expected);
}

/**
 * A value several profiles hold gives every one of them, in the order of the
 * data file, which is ascending profile id.
 */
TEST_F(HashValueProfilesTests, SeveralProfilesInOrder) {
	Manager manager;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "ScreenPixelsWidth" }));
	ASSERT_NE(nullptr, getIndex(manager));
	const ValueProfileIds found = readValueProfileIds(manager);
	const PropertyValues values = getValues(manager, "ScreenPixelsWidth");
	const int i = findValue(values, found, 2, true);
	ASSERT_GE(i, 0) <<
		"No ScreenPixelsWidth is held by more than two profiles.";
	const ProfileIds ids = getIds(
		manager,
		"ScreenPixelsWidth",
		values.names[i]);
	EXPECT_EQ(
		expectedFor(found, values.firstValueIndex + (uint32_t)i),
		ids);
	for (size_t p = 1; p < ids.size(); p++) {
		EXPECT_LT(ids[p - 1], ids[p]) << "The profiles must be in order.";
	}
	expectLookups(
		manager,
		"ScreenPixelsWidth",
		values.names[i],
		expectedFor(found, values.firstValueIndex + (uint32_t)i));
}

/** A value the property does not have gives no profiles. */
TEST_F(HashValueProfilesTests, ValueNotFound) {
	Manager indexed, scanned;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		indexed,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "IsMobile" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		scanned,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	ASSERT_NE(nullptr, getIndex(indexed));
	ASSERT_EQ(nullptr, getIndex(scanned));
	for (Manager *manager : { &indexed, &scanned }) {
		expectLookups(*manager, "IsMobile", "NoSuchValue", ProfileIds());
		expectLookups(*manager, "IsMobile", "", ProfileIds());

		// Values are matched exactly, so a value in the wrong case is not
		// found.
		expectLookups(*manager, "IsMobile", "true", ProfileIds());
	}
}

/**
 * A property that is not in the data file, or is in a different case, gives
 * no profiles, whether or not it was named to index, as names are matched
 * exactly. A name the data file has no property of is kept with no property.
 */
TEST_F(HashValueProfilesTests, PropertyNotFound) {
	Manager indexed, scanned;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		indexed,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "IsMobile", "NoSuchProperty", "ismobile" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		scanned,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	const fiftyoneDegreesHashValueProfiles *index = getIndex(indexed);
	ASSERT_NE(nullptr, index);
	ASSERT_EQ(3u, index->propertiesCount);
	EXPECT_NE(UINT32_MAX, index->propertyIndexes[0]);
	EXPECT_EQ(UINT32_MAX, index->propertyIndexes[1]);
	EXPECT_EQ(UINT32_MAX, index->propertyIndexes[2]);
	EXPECT_EQ(index->firstKeys[1], index->firstKeys[2]);
	EXPECT_EQ(index->firstKeys[2], index->firstKeys[3]);
	const PropertyValues values = getValues(indexed, "IsMobile");
	ASSERT_FALSE(values.names.empty());
	for (Manager *manager : { &indexed, &scanned }) {
		expectLookups(*manager, "NoSuchProperty", "True", ProfileIds());
		expectLookups(*manager, "ismobile", values.names[0], ProfileIds());
		expectLookups(*manager, "ISMOBILE", values.names[0], ProfileIds());
	}
}

/**
 * A property is indexed whether or not it is required. A required property
 * that is not named is answered by reading every profile, with the same
 * profiles.
 */
TEST_F(HashValueProfilesTests, PropertiesNotRequired) {
	Manager manager, all;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		"IsMobile",
		path,
		{ "BrowserName" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		all,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	{
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(manager.get());
		EXPECT_EQ(1u, dataSet->b.b.available->count);
		fiftyoneDegreesDataSetHashRelease(dataSet);
	}
	const fiftyoneDegreesHashValueProfiles *index = getIndex(manager);
	ASSERT_NE(nullptr, index);
	ASSERT_EQ(1u, index->propertiesCount);
	EXPECT_STREQ("BrowserName", index->names[0]);
	EXPECT_NE(UINT32_MAX, index->propertyIndexes[0]);
	const ValueProfileIds found = readValueProfileIds(all);
	for (const char *property : { "BrowserName", "IsMobile" }) {
		const PropertyValues values = getValues(all, property);
		const int i = findValue(values, found, 1, true);
		ASSERT_GE(i, 0) << "No " << property <<
			" is held by more than one profile.";
		const ProfileIds expected =
			expectedFor(found, values.firstValueIndex + (uint32_t)i);
		expectLookups(manager, property, values.names[i], expected);
		expectLookups(all, property, values.names[i], expected);
	}
}

/**
 * The names are copied, in the order given without nulls or repeats, so the
 * caller's strings can change once the data set is initialised.
 */
TEST_F(HashValueProfilesTests, NamesCopied) {
	char width[] = "ScreenPixelsWidth";
	char mobile[] = "IsMobile";
	Manager manager;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ width, nullptr, mobile, width, "IsMobile" }));
	const fiftyoneDegreesHashValueProfiles *index = getIndex(manager);
	ASSERT_NE(nullptr, index);
	ASSERT_EQ(2u, index->propertiesCount);
	EXPECT_NE((const char*)width, index->names[0]);
	EXPECT_NE((const char*)mobile, index->names[1]);
	width[0] = 'X';
	mobile[0] = 'X';
	EXPECT_STREQ("ScreenPixelsWidth", index->names[0]);
	EXPECT_STREQ("IsMobile", index->names[1]);
	const ValueProfileIds found = readValueProfileIds(manager);
	const PropertyValues values = getValues(manager, "ScreenPixelsWidth");
	const int i = findValue(values, found, 2, true);
	ASSERT_GE(i, 0);
	expectLookups(
		manager,
		"ScreenPixelsWidth",
		values.names[i],
		expectedFor(found, values.firstValueIndex + (uint32_t)i));
}

/**
 * Nothing is indexed when no property is named, being no names, a count of
 * none, or only nulls, from a file or from memory.
 */
TEST_F(HashValueProfilesTests, NothingIndexedWithoutNames) {
	std::vector<unsigned char> data;
	ASSERT_NO_FATAL_FAILURE(readFile(path, data));
	const char *nulls[] = { nullptr, nullptr };
	const char *isMobile[] = { "IsMobile" };
	const struct {
		const char *description;
		const char **names;
		int count;
	} cases[] = {
		{ "No names", nullptr, 0 },
		{ "A count of none", isMobile, 0 },
		{ "A count below none", isMobile, -1 },
		{ "Only nulls", nulls, 2 },
	};
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	config.b.b.freeData = false;
	for (const auto &c : cases) {
		for (bool fromMemory : { false, true }) {
			Manager manager;
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			const fiftyoneDegreesStatusCode status = fromMemory ?
				fiftyoneDegreesHashInitManagerFromMemoryWithIndexedProperties(
					manager.get(),
					&config,
					nullptr,
					data.data(),
					(long)data.size(),
					c.names,
					c.count,
					exception) :
				fiftyoneDegreesHashInitManagerFromFileWithIndexedProperties(
					manager.get(),
					&config,
					nullptr,
					path.c_str(),
					c.names,
					c.count,
					exception);
			ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status) <<
				c.description;
			manager.initialised = true;
			EXPECT_EQ(nullptr, getIndex(manager)) << c.description <<
				(fromMemory ? " from memory" : " from a file");
		}
	}
}

/**
 * A profile holding several values of a property is given for each of them.
 * The Lite file has no list property, so a copy is made in which one profile
 * holds two values of ScreenPixelsWidth in place of its ScreenPixelsHeight.
 */
TEST_F(HashValueProfilesTests, ListOfValues) {
	ListData list;
	{
		std::vector<unsigned char> original;
		ASSERT_NO_FATAL_FAILURE(readFile(path, original));
		ASSERT_NO_FATAL_FAILURE(makeListData(
			original,
			"ScreenPixelsWidth",
			"ScreenPixelsHeight",
			list));
	}
	{
		Manager indexed, scanned;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			indexed,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			list.data,
			{ "ScreenPixelsWidth", "ScreenPixelsHeight" }));
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			scanned,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			list.data));
		ASSERT_NE(nullptr, getIndex(indexed));
		const ValueProfileIds found = readValueProfileIds(indexed);
		const PropertyValues width = getValues(indexed, "ScreenPixelsWidth");
		const PropertyValues height = getValues(indexed, "ScreenPixelsHeight");

		// The profile now holds the added width as well as the one it kept,
		// and no longer holds the height it lost.
		EXPECT_TRUE(contains(expectedFor(found, list.kept), list.profileId));
		EXPECT_TRUE(contains(expectedFor(found, list.added), list.profileId));
		EXPECT_FALSE(
			contains(expectedFor(found, list.removed), list.profileId));
		const struct {
			const char *property;
			const PropertyValues *values;
			uint32_t valueIndex;
		} cases[] = {
			{ "ScreenPixelsWidth", &width, list.kept },
			{ "ScreenPixelsWidth", &width, list.added },
			{ "ScreenPixelsHeight", &height, list.removed },
		};
		for (const auto &c : cases) {
			const std::string &name =
				c.values->names[c.valueIndex - c.values->firstValueIndex];
			const ProfileIds expected = expectedFor(found, c.valueIndex);
			expectLookups(indexed, c.property, name, expected);
			expectLookups(scanned, c.property, name, expected);
		}
	}
}

/**
 * A reload from memory indexes the same names again, from the new data,
 * rather than keeping the index of the data set it replaces. A data set that
 * indexes nothing still indexes nothing after a reload.
 */
TEST_F(HashValueProfilesTests, ReloadFromMemoryKeepsNames) {
	std::vector<unsigned char> original;
	ListData list;
	ASSERT_NO_FATAL_FAILURE(readFile(path, original));
	ASSERT_NO_FATAL_FAILURE(makeListData(
		original,
		"ScreenPixelsWidth",
		"ScreenPixelsHeight",
		list));
	{
		Manager manager;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			manager,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			original,
			{ "ScreenPixelsWidth", "NoSuchProperty" }));
		const PropertyValues width = getValues(manager, "ScreenPixelsWidth");
		const std::string added =
			width.names[list.added - width.firstValueIndex];
		EXPECT_FALSE(contains(
			getIds(manager, "ScreenPixelsWidth", added),
			list.profileId));

		// Each reload replaces the data set and builds a new index for the
		// same names, whilst the data set held here keeps its own.
		for (std::vector<unsigned char> *data : { &list.data, &original }) {
			fiftyoneDegreesDataSetHash *before =
				fiftyoneDegreesDataSetHashGet(manager.get());
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				fiftyoneDegreesHashReloadManagerFromMemory(
					manager.get(),
					data->data(),
					(long)data->size(),
					exception));
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
			fiftyoneDegreesDataSetHash *after =
				fiftyoneDegreesDataSetHashGet(manager.get());
			EXPECT_NE(before, after);
			EXPECT_NE(nullptr, before->valueProfiles);
			const fiftyoneDegreesHashValueProfiles *index =
				after->valueProfiles;
			EXPECT_NE(before->valueProfiles, index);
			if (index != nullptr) {
				EXPECT_EQ(2u, index->propertiesCount);
				EXPECT_STREQ("ScreenPixelsWidth", index->names[0]);
				EXPECT_STREQ("NoSuchProperty", index->names[1]);
				EXPECT_NE(UINT32_MAX, index->propertyIndexes[0]);
				EXPECT_EQ(UINT32_MAX, index->propertyIndexes[1]);
			}
			else {
				ADD_FAILURE() << "The reload indexed nothing.";
			}
			fiftyoneDegreesDataSetHashRelease(after);
			fiftyoneDegreesDataSetHashRelease(before);
			const ValueProfileIds found = readValueProfileIds(
				manager,
				list.added,
				list.added);
			const ProfileIds ids = getIds(manager, "ScreenPixelsWidth", added);
			EXPECT_EQ(expectedFor(found, list.added), ids);
			EXPECT_EQ(data == &list.data, contains(ids, list.profileId));
		}
	}
	{
		Manager manager;
		ASSERT_NO_FATAL_FAILURE(initFromMemory(
			manager,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			original));
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashReloadManagerFromMemory(
				manager.get(),
				list.data.data(),
				(long)list.data.size(),
				exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		EXPECT_EQ(nullptr, getIndex(manager));
	}
}

/**
 * A reload from the original file, or from a file, indexes the same names
 * again. A data set that indexes nothing still indexes nothing after one.
 */
TEST_F(HashValueProfilesTests, ReloadFromFileKeepsNames) {
	Manager manager, none;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "ScreenPixelsWidth" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		none,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	const ValueProfileIds found = readValueProfileIds(manager);
	const PropertyValues values = getValues(manager, "ScreenPixelsWidth");
	const int i = findValue(values, found, 2, true);
	ASSERT_GE(i, 0);
	for (int reload = 0; reload < 2; reload++) {
		const char *context = reload == 0 ?
			"From the original file" : "From a file";
		for (Manager *m : { &manager, &none }) {
			fiftyoneDegreesDataSetHash *before =
				fiftyoneDegreesDataSetHashGet(m->get());
			FIFTYONE_DEGREES_EXCEPTION_CREATE;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				reload == 0 ?
				fiftyoneDegreesHashReloadManagerFromOriginalFile(
					m->get(),
					exception) :
				fiftyoneDegreesHashReloadManagerFromFile(
					m->get(),
					path.c_str(),
					exception)) << context;
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << context;
			fiftyoneDegreesDataSetHash *after =
				fiftyoneDegreesDataSetHashGet(m->get());
			EXPECT_NE(before, after) << context;
			if (m == &none) {
				EXPECT_EQ(nullptr, after->valueProfiles) << context;
			}
			else if (after->valueProfiles == nullptr) {
				ADD_FAILURE() << context << ", the reload indexed nothing.";
			}
			else {
				EXPECT_NE(before->valueProfiles, after->valueProfiles) <<
					context;
				EXPECT_EQ(1u, after->valueProfiles->propertiesCount) <<
					context;
				EXPECT_STREQ(
					"ScreenPixelsWidth",
					after->valueProfiles->names[0]) << context;
			}
			fiftyoneDegreesDataSetHashRelease(after);
			fiftyoneDegreesDataSetHashRelease(before);
			expectLookups(
				*m,
				"ScreenPixelsWidth",
				values.names[i],
				expectedFor(found, values.firstValueIndex + (uint32_t)i),
				context);
		}
	}
}

/**
 * Every performance profile gives the same profiles. The index is built when
 * properties are named, whatever the performance profile and whatever
 * propertyValueIndex, which decides only the index of first values for
 * profiles and properties.
 */
TEST_F(HashValueProfilesTests, EveryPerformanceProfile) {
	ValueProfileIds found;
	PropertyValues width, isMobile;
	{
		Manager reader;
		ASSERT_NO_FATAL_FAILURE(initFromFile(
			reader,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			path));
		found = readValueProfileIds(reader);
		width = getValues(reader, "ScreenPixelsWidth");
		isMobile = getValues(reader, "IsMobile");
	}
	const int one = findValue(width, found, 1, false);
	const int several = findValue(width, found, 2, true);
	ASSERT_GE(one, 0);
	ASSERT_GE(several, 0);
	for (const auto &profile : performanceProfiles) {
		for (bool valueIndex : { false, true }) {
			for (bool named : { false, true }) {
				const std::string context = std::string(profile.name) +
					(valueIndex ? ", propertyValueIndex on" :
						", propertyValueIndex off") +
					(named ? ", properties named, " : ", none named, ");
				fiftyoneDegreesConfigHash config = *profile.config;
				config.b.b.propertyValueIndex = valueIndex;
				Manager manager;
				if (named) {
					ASSERT_NO_FATAL_FAILURE(initFromFile(
						manager,
						config,
						nullptr,
						path,
						{ "ScreenPixelsWidth", "IsMobile" }));
				}
				else {
					// The init function that takes no names indexes nothing.
					FIFTYONE_DEGREES_EXCEPTION_CREATE;
					ASSERT_EQ(
						FIFTYONE_DEGREES_STATUS_SUCCESS,
						fiftyoneDegreesHashInitManagerFromFile(
							manager.get(),
							&config,
							nullptr,
							path.c_str(),
							exception)) << context;
					manager.initialised = true;
				}
				fiftyoneDegreesDataSetHash *dataSet =
					fiftyoneDegreesDataSetHashGet(manager.get());
				EXPECT_EQ(named, dataSet->valueProfiles != nullptr) <<
					context;
				EXPECT_EQ(
					valueIndex,
					dataSet->b.b.indexPropertyProfile != nullptr) << context;
				fiftyoneDegreesDataSetHashRelease(dataSet);
				expectLookups(
					manager,
					"ScreenPixelsWidth",
					width.names[one],
					expectedFor(found, width.firstValueIndex + (uint32_t)one),
					context);
				expectLookups(
					manager,
					"ScreenPixelsWidth",
					width.names[several],
					expectedFor(
						found,
						width.firstValueIndex + (uint32_t)several),
					context);
				for (size_t i = 0; i < isMobile.names.size(); i++) {
					expectLookups(
						manager,
						"IsMobile",
						isMobile.names[i],
						expectedFor(
							found,
							isMobile.firstValueIndex + (uint32_t)i),
						context);
				}
				expectLookups(
					manager,
					"IsMobile",
					"NoSuchValue",
					ProfileIds(),
					context);
			}
		}
	}
}

/**
 * For every value of every property NativeModel and TAC would be among, being
 * those of the IsMobile component, the index gives the profiles that reading
 * every profile gives, and those are the profiles the value really has.
 */
TEST_F(HashValueProfilesTests, AgreesWithReadingEveryProfile) {
	std::vector<std::string> properties;
	{
		Manager reader;
		ASSERT_NO_FATAL_FAILURE(initFromFile(
			reader,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			path));
		properties = getPropertyNames(
			reader,
			getValues(reader, "IsMobile").componentIndex);
	}
	ASSERT_FALSE(properties.empty());
	Manager indexed, scanned;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		indexed,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		pointTo(properties)));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		scanned,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	const fiftyoneDegreesHashValueProfiles *index = getIndex(indexed);
	ASSERT_NE(nullptr, index);
	EXPECT_EQ(properties.size(), (size_t)index->propertiesCount);
	ASSERT_EQ(nullptr, getIndex(scanned));
	const ValueProfileIds found = readValueProfileIds(indexed);
	size_t checked = 0, failed = 0;
	std::string difference;
	for (const std::string &property : properties) {
		const PropertyValues values = getValues(indexed, property);
		for (size_t i = 0; i < values.names.size(); i++) {
			const ProfileIds expected =
				expectedFor(found, values.firstValueIndex + (uint32_t)i);
			if (lookupsMatch(
					indexed,
					property,
					values.names[i],
					expected,
					difference) == false ||
				lookupsMatch(
					scanned,
					property,
					values.names[i],
					expected,
					difference) == false) {
				failed++;
			}
			checked++;
		}
	}
	EXPECT_GT(checked, 0u);
	EXPECT_EQ(0u, failed) << failed << " of " << checked << " values " <<
		"differ, the first being " << difference;
}

/**
 * With every property in the data file named, every value gives the profiles
 * it really has, and the arrays of the index are as hash.h describes them.
 */
TEST_F(HashValueProfilesTests, EveryValueOfEveryProperty) {
	std::vector<std::string> properties;
	{
		Manager reader;
		ASSERT_NO_FATAL_FAILURE(initFromFile(
			reader,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			path));
		properties = getPropertyNames(reader, -1);
	}
	Manager manager;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		manager,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		pointTo(properties)));
	const fiftyoneDegreesHashValueProfiles *index = getIndex(manager);
	ASSERT_NE(nullptr, index);
	ASSERT_EQ(properties.size(), (size_t)index->propertiesCount);
	EXPECT_EQ(index->keysCount, index->firstKeys[index->propertiesCount]);
	EXPECT_EQ(index->profilesCount, index->starts[index->keysCount]);
	EXPECT_EQ(0u, index->firstKeys[0]);
	EXPECT_EQ(0u, index->starts[0]);
	for (uint32_t k = 0; k < index->keysCount; k++) {
		ASSERT_LE(index->starts[k], index->starts[k + 1]);
		for (uint32_t p = index->starts[k] + 1;
			p < index->starts[k + 1];
			p++) {
			ASSERT_LT(index->profiles[p - 1], index->profiles[p]);
		}
	}

	const ValueProfileIds found = readValueProfileIds(manager);
	size_t checked = 0, failed = 0, pairs = 0;
	std::string difference;
	for (uint32_t i = 0; i < index->propertiesCount; i++) {
		const std::string &property = properties[i];
		EXPECT_EQ(property, std::string(index->names[i]));
		EXPECT_EQ(i, index->propertyIndexes[i]) <<
			"Each property is the one at its place in the data file.";
		const PropertyValues values = getValues(manager, property);
		EXPECT_EQ(values.firstValueIndex, index->firstValueIndexes[i]);
		EXPECT_EQ(
			values.names.size(),
			(size_t)(index->firstKeys[i + 1] - index->firstKeys[i]));
		for (size_t v = 0; v < values.names.size(); v++) {
			const ProfileIds expected =
				expectedFor(found, values.firstValueIndex + (uint32_t)v);
			const ProfileIds ids = getIds(manager, property, values.names[v]);
			if (ids != expected && difference.empty()) {
				difference = property + " '" + values.names[v] + "' gave " +
					describe(ids) + " not " + describe(expected);
			}
			failed += ids != expected ? 1 : 0;
			pairs += expected.size();
			checked++;
		}
	}
	EXPECT_EQ(0u, failed) << failed << " of " << checked << " values " <<
		"differ, the first being " << difference;
	EXPECT_EQ((size_t)index->profilesCount, pairs);
}

/**
 * An array shorter than the number of profiles gets the first ids, and the
 * number returned is still every profile.
 */
TEST_F(HashValueProfilesTests, ArrayTooShort) {
	Manager indexed, scanned;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		indexed,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path,
		{ "ScreenPixelsWidth" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		scanned,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		path));
	ASSERT_NE(nullptr, getIndex(indexed));
	const ValueProfileIds found = readValueProfileIds(indexed);
	const PropertyValues values = getValues(indexed, "ScreenPixelsWidth");
	const int i = findValue(values, found, 2, true);
	ASSERT_GE(i, 0);
	const ProfileIds expected =
		expectedFor(found, values.firstValueIndex + (uint32_t)i);
	for (Manager *manager : { &indexed, &scanned }) {
		ProfileIds ids(3, UINT32_MAX);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		EXPECT_EQ(
			(uint32_t)expected.size(),
			fiftyoneDegreesHashGetProfileIdsForPropertyAndValue(
				manager->get(),
				"ScreenPixelsWidth",
				values.names[i].c_str(),
				ids.data(),
				2,
				exception));
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		EXPECT_EQ(expected[0], ids[0]);
		EXPECT_EQ(expected[1], ids[1]);
		EXPECT_EQ(UINT32_MAX, ids[2]) << "Only the length given is set.";
	}
}

/**
 * The C++ engine indexes the properties its configuration names, from a file
 * and from memory, and indexes them again on a refresh. It indexes none by
 * default, even with the property value index on, and gives the same ids
 * either way.
 */
TEST_F(HashValueProfilesTests, EngineHashGetsProfileIds) {
	ValueProfileIds found;
	PropertyValues values;
	std::vector<unsigned char> data;
	ASSERT_NO_FATAL_FAILURE(readFile(path, data));
	{
		Manager reader;
		ASSERT_NO_FATAL_FAILURE(initFromFile(
			reader,
			fiftyoneDegreesHashInMemoryConfig,
			nullptr,
			path));
		found = readValueProfileIds(reader);
		values = getValues(reader, "ScreenPixelsWidth");
	}
	const int i = findValue(values, found, 2, true);
	ASSERT_GE(i, 0);
	const ProfileIds expected =
		expectedFor(found, values.firstValueIndex + (uint32_t)i);
	RequiredPropertiesConfig properties;
	for (bool named : { false, true }) {
		for (bool fromMemory : { false, true }) {
			const std::string context = std::string(
				named ? "ScreenPixelsWidth named" : "None named") +
				(fromMemory ? ", from memory" : ", from a file");
			ConfigHash config;
			EXPECT_TRUE(config.getIndexedProperties().empty()) << context;
			config.setMaxPerformance();
			config.setPropertyValueIndex(true);
			if (named) {
				config.setIndexedProperties({ "ScreenPixelsWidth" });
				EXPECT_EQ(
					std::vector<std::string>({ "ScreenPixelsWidth" }),
					config.getIndexedProperties()) << context;
			}
			std::unique_ptr<IndexedEngine> engine(fromMemory ?
				new IndexedEngine(
					data.data(),
					(long)data.size(),
					&config,
					&properties) :
				new IndexedEngine(path, &config, &properties));
			EXPECT_EQ(named, engine->hasIndex()) << context;
			EXPECT_EQ(
				expected,
				engine->getProfileIdsForPropertyValue(
					"ScreenPixelsWidth",
					values.names[i])) << context;
			EXPECT_TRUE(engine->getProfileIdsForPropertyValue(
				"ScreenPixelsWidth",
				"NoSuchValue").empty()) << context;
			EXPECT_TRUE(engine->getProfileIdsForPropertyValue(
				"NoSuchProperty",
				values.names[i]).empty()) << context;
			if (fromMemory) {
				engine->refreshData(data.data(), (long)data.size());
			}
			else {
				engine->refreshData();
			}
			EXPECT_EQ(named, engine->hasIndex()) << context <<
				", after a refresh";
			EXPECT_EQ(
				expected,
				engine->getProfileIdsForPropertyValue(
					"ScreenPixelsWidth",
					values.names[i])) << context << ", after a refresh";
		}
	}
}

/**
 * When an Enterprise data file is present, every NativeModel and TAC gives the
 * profiles it really has. Reading every profile is compared for a sample of
 * the values, as doing it for all of them would take minutes.
 */
TEST_F(HashValueProfilesTests, EnterpriseNativeModelAndTac) {
	std::string enterprise;
	for (const char *name : { "TAC-HashV41.hash",
		"51Degrees-EnterpriseV4.1.hash" }) {
		if (enterprise.empty()) {
			enterprise = GetFilePath(_dataFolderName, name);
		}
	}
	// The googletest of the Visual Studio projects has no GTEST_SKIP, so the
	// test returns, as the other tests that need an Enterprise file do.
	if (enterprise.empty()) {
		std::cout << "No Enterprise data file is present, so NativeModel " <<
			"and TAC are not checked.\n";
		return;
	}
	Manager indexed, scanned;
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		indexed,
		fiftyoneDegreesHashInMemoryConfig,
		nullptr,
		enterprise,
		{ "NativeModel", "TAC" }));
	ASSERT_NO_FATAL_FAILURE(initFromFile(
		scanned,
		fiftyoneDegreesHashLowMemoryConfig,
		nullptr,
		enterprise));
	ASSERT_NE(nullptr, getIndex(indexed));
	ASSERT_EQ(nullptr, getIndex(scanned));
	const size_t samples = 10;
	size_t properties = 0;
	for (const char *property : { "NativeModel", "TAC" }) {
		const PropertyValues values = getValues(indexed, property);
		if (values.names.empty()) {
			std::cout << "The data file has no " << property << ".\n";
			continue;
		}
		properties++;
		const ValueProfileIds found = readValueProfileIds(
			indexed,
			values.firstValueIndex,
			values.firstValueIndex + (uint32_t)values.names.size() - 1);
		size_t failed = 0, several = 0;
		std::string difference;
		for (size_t i = 0; i < values.names.size(); i++) {
			const ProfileIds expected =
				expectedFor(found, values.firstValueIndex + (uint32_t)i);
			const ProfileIds ids = getIds(indexed, property, values.names[i]);
			if (ids != expected && difference.empty()) {
				difference = std::string(property) + " '" + values.names[i] +
					"' gave " + describe(ids) + " not " + describe(expected);
			}
			failed += ids != expected ? 1 : 0;
			several += expected.size() > 1 ? 1 : 0;
		}
		EXPECT_EQ(0u, failed) << failed << " of " << values.names.size() <<
			" values of " << property << " differ, the first being " <<
			difference;
		std::cout << values.names.size() << " values of " << property <<
			", " << several << " held by more than one profile.\n";
		const size_t step = values.names.size() / samples + 1;
		for (size_t i = 0; i < values.names.size(); i += step) {
			expectLookups(
				scanned,
				property,
				values.names[i],
				expectedFor(found, values.firstValueIndex + (uint32_t)i));
		}
	}
	EXPECT_GT(properties, 0u) << "The data file has neither property.";
}
