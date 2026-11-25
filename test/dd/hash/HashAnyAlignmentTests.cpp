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
 * @file HashAnyAlignmentTests.cpp
 * @brief Reads the Lite data file from memory placed at each byte offset from
 * 0 to 7 of a buffer, so every packed record of the file starts at each
 * alignment in turn, and checks that each offset gives the same answers.
 *
 * The data file packs its records with no padding, so a number in one can
 * start at any byte. Built with the alignment check of the undefined
 * behavior sanitizer, a read that takes such a number to be aligned is
 * reported.
 *
 * Each offset reads the data set header, the component key value pairs, the
 * root nodes and node hash records of the graphs, the values, the value
 * indexes of the profiles, both as the index of profiles by value builds
 * and as the C++ metadata reads them, and the profile offsets.
 */

#include "../../common/pch.h"
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/MetaDataHash.hpp"
#include "../../../src/dd/hash/hash.h"

namespace Hash = FiftyoneDegrees::DeviceDetection::Hash;
namespace Common = FiftyoneDegrees::Common;

using std::string;
using std::unique_ptr;
using std::vector;
using Values =
	Common::Collection<Common::ValueMetaDataKey, Common::ValueMetaData>;
using Profiles = Common::Collection<uint32_t, Common::ProfileMetaData>;

namespace {

/** The byte offsets of the buffer the data file is placed at. */
const size_t hashAnyAlignmentOffsets = 8;

/** The profiles whose values the C++ metadata reads, spread across them. */
const uint32_t hashAnyAlignmentProfiles = 100;

/** The size of the buffer each value string is written to. */
const size_t hashAnyAlignmentTextBytes = 64 * 1024;

/** The properties the index of profiles by value is built for. */
const char *hashAnyAlignmentIndexed[] = { "PlatformName", "BrowserName" };

/** A value of each indexed property, in the same order. */
const char *hashAnyAlignmentIndexedValues[] = { "Android", "Chrome" };

/** User-Agents that take different routes through the graphs. */
const char *hashAnyAlignmentUserAgents[] = {
	"Mozilla/5.0 (Linux; Android 13; SM-S901B) AppleWebKit/537.36 "
		"(KHTML, like Gecko) Chrome/112.0.0.0 Mobile Safari/537.36",
	"Mozilla/5.0 (iPhone; CPU iPhone OS 16_5 like Mac OS X) "
		"AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.5 "
		"Mobile/15E148 Safari/604.1",
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
		"(KHTML, like Gecko) Chrome/118.0.0.0 Safari/537.36 Edg/118.0.2088.46",
	"Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:109.0) "
		"Gecko/20100101 Firefox/118.0",
	"Mozilla/5.0 (compatible; Googlebot/2.1; "
		"+http://www.google.com/bot.html)",
	"NoSuchUserAgent"
};

/** Frees the manager, and with it the data set, at the end of a scope. */
std::shared_ptr<fiftyoneDegreesResourceManager> hashAnyAlignmentManager() {
	return std::shared_ptr<fiftyoneDegreesResourceManager>(
		new fiftyoneDegreesResourceManager(),
		[](fiftyoneDegreesResourceManager *manager) {
			if (manager->active != nullptr) {
				fiftyoneDegreesResourceManagerFree(manager);
			}
			delete manager;
		});
}

/** Reads the whole file. */
void hashAnyAlignmentRead(const string &path, vector<unsigned char> &data) {
	FILE *file = nullptr;
	fiftyoneDegreesFileOpen(path.c_str(), &file);
	ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
	fseek(file, 0, SEEK_END);
	data.resize((size_t)ftell(file));
	rewind(file);
	const size_t bytes = fread(data.data(), 1, data.size(), file);
	fclose(file);
	ASSERT_EQ(data.size(), bytes) << "Could not read '" << path << "'.";
}

/** Adds the values of every property for each User-Agent. */
void hashAnyAlignmentResults(
	fiftyoneDegreesResourceManager *manager,
	std::stringstream &answers) {
	vector<char> buffer(hashAnyAlignmentTextBytes);
	char separator[] = "|";
	fiftyoneDegreesResultsHash *results =
		fiftyoneDegreesResultsHashCreate(manager, 1, 0);
	ASSERT_NE(nullptr, results);
	for (const char *userAgent : hashAnyAlignmentUserAgents) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			userAgent,
			strlen(userAgent),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << userAgent;
		fiftyoneDegreesResultsHashGetValuesStringAllProperties(
			results,
			buffer.data(),
			buffer.size(),
			separator,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << userAgent;
		answers << buffer.data() << "\n";
		fiftyoneDegreesHashGetDeviceIdFromResults(
			results,
			buffer.data(),
			buffer.size(),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << userAgent;
		answers << buffer.data() << "\n";
	}
	fiftyoneDegreesResultsHashFree(results);
}

/** Adds the ids of the profiles holding each indexed value. */
void hashAnyAlignmentProfileIds(
	fiftyoneDegreesResourceManager *manager,
	std::stringstream &answers) {
	for (size_t i = 0; i < sizeof(hashAnyAlignmentIndexed) /
		sizeof(hashAnyAlignmentIndexed[0]); i++) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const uint32_t count =
			fiftyoneDegreesHashGetProfileIdsForPropertyAndValue(
				manager,
				hashAnyAlignmentIndexed[i],
				hashAnyAlignmentIndexedValues[i],
				nullptr,
				0,
				exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		EXPECT_LT(0u, count) << "No profile holds " <<
			hashAnyAlignmentIndexed[i] << " " <<
			hashAnyAlignmentIndexedValues[i];
		vector<uint32_t> ids(count);
		fiftyoneDegreesHashGetProfileIdsForPropertyAndValue(
			manager,
			hashAnyAlignmentIndexed[i],
			hashAnyAlignmentIndexedValues[i],
			ids.data(),
			count,
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		answers << hashAnyAlignmentIndexed[i] << ":";
		for (const uint32_t id : ids) {
			answers << " " << id;
		}
		answers << "\n";
	}
}

/**
 * Adds the values of a sample of the profiles, read by position and found
 * again by key through the C++ metadata.
 */
void hashAnyAlignmentProfileValues(
	std::shared_ptr<fiftyoneDegreesResourceManager> manager,
	std::stringstream &answers) {
	Hash::MetaDataHash metaData(manager);
	unique_ptr<Profiles> profiles(metaData.getProfiles());
	const uint32_t count = profiles->getSize();
	ASSERT_LT(0u, count);
	const uint32_t step = count / hashAnyAlignmentProfiles + 1;
	for (uint32_t p = 0; p < count; p += step) {
		unique_ptr<Common::ProfileMetaData> profile(profiles->getByIndex(p));
		ASSERT_TRUE(profile != nullptr) << p;
		unique_ptr<Values> values(
			metaData.getValuesForProfile(profile.get()));
		answers << profile->getProfileId() << ":";
		for (uint32_t v = 0; v < values->getSize(); v++) {
			unique_ptr<Common::ValueMetaData> value(values->getByIndex(v));
			ASSERT_TRUE(value != nullptr) << p << " " << v;
			unique_ptr<Common::ValueMetaData> byKey(
				values->getByKey(value->getKey()));
			EXPECT_TRUE(byKey != nullptr) << p << " " << v;
			answers << " " << value->getKey().getPropertyName() << "=" <<
				value->getName();
		}
		answers << "\n";
	}
}

/** Gets the answers of the data file placed at the offset of a buffer. */
void hashAnyAlignmentAnswers(
	const vector<unsigned char> &file,
	size_t offset,
	string &answers) {
	vector<unsigned char> buffer(file.size() + hashAnyAlignmentOffsets);
	memcpy(buffer.data() + offset, file.data(), file.size());
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	config.b.b.freeData = false;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	std::shared_ptr<fiftyoneDegreesResourceManager> manager =
		hashAnyAlignmentManager();
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesHashInitManagerFromMemoryWithIndexedProperties(
			manager.get(),
			&config,
			&properties,
			buffer.data() + offset,
			(long)file.size(),
			hashAnyAlignmentIndexed,
			(int)(sizeof(hashAnyAlignmentIndexed) /
				sizeof(hashAnyAlignmentIndexed[0])),
			exception)) << "Offset " << offset;
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << "Offset " << offset;
	std::stringstream found;
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentResults(manager.get(), found));
	ASSERT_NO_FATAL_FAILURE(
		hashAnyAlignmentProfileIds(manager.get(), found));
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentProfileValues(manager, found));
	answers = found.str();
}

}

class HashAnyAlignmentTests : public Base {
};

/**
 * The data file placed at each offset of a buffer gives the same values,
 * profile ids and profile values as at the start of the buffer.
 */
TEST_F(HashAnyAlignmentTests, MemoryAtEveryOffset) {
	const string path = GetFilePath(_dataFolderName, "51Degrees-LiteV4.1.hash");
	ASSERT_FALSE(path.empty()) << "The Lite data file was not found.";
	vector<unsigned char> file;
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentRead(path, file));
	string expected;
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentAnswers(file, 0, expected));
	ASSERT_FALSE(expected.empty());
	for (size_t offset = 1; offset < hashAnyAlignmentOffsets; offset++) {
		string answers;
		ASSERT_NO_FATAL_FAILURE(
			hashAnyAlignmentAnswers(file, offset, answers));
		EXPECT_TRUE(expected == answers) << "Offset " << offset <<
			" gives different answers from offset 0.";
	}
}

/**
 * A data set read from the file, with the least memory, gives the same values
 * as one in memory. The header counts are set while the collections are made
 * from the file, and the data set holds the header at a position that is not
 * 4 byte aligned.
 */
TEST_F(HashAnyAlignmentTests, FileMatchesMemory) {
	const string path = GetFilePath(_dataFolderName, "51Degrees-LiteV4.1.hash");
	ASSERT_FALSE(path.empty()) << "The Lite data file was not found.";
	vector<unsigned char> file;
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentRead(path, file));
	string expected;
	{
		std::stringstream found;
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
		config.b.b.freeData = false;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		std::shared_ptr<fiftyoneDegreesResourceManager> manager =
			hashAnyAlignmentManager();
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromMemory(
				manager.get(),
				&config,
				&properties,
				file.data(),
				(long)file.size(),
				exception));
		ASSERT_NO_FATAL_FAILURE(
			hashAnyAlignmentResults(manager.get(), found));
		expected = found.str();
	}
	std::stringstream found;
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	std::shared_ptr<fiftyoneDegreesResourceManager> manager =
		hashAnyAlignmentManager();
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesHashInitManagerFromFile(
			manager.get(),
			&config,
			&properties,
			path.c_str(),
			exception));
	ASSERT_NO_FATAL_FAILURE(hashAnyAlignmentResults(manager.get(), found));
	EXPECT_TRUE(expected == found.str());
}

/**
 * The engine collects the id of each profile holding a value that was not
 * indexed by reading every profile, and gives the same ids as the index.
 */
TEST_F(HashAnyAlignmentTests, EngineProfileIdsWithoutIndex) {
	const string path = GetFilePath(_dataFolderName, "51Degrees-LiteV4.1.hash");
	ASSERT_FALSE(path.empty()) << "The Lite data file was not found.";
	Common::RequiredPropertiesConfig properties;
	Hash::ConfigHash plain;
	plain.setMaxPerformance();
	plain.setPropertyValueIndex(true);
	Hash::ConfigHash indexed;
	indexed.setMaxPerformance();
	indexed.setPropertyValueIndex(true);
	indexed.setIndexedProperties({ hashAnyAlignmentIndexed[0] });
	Hash::EngineHash withoutIndex(path, &plain, &properties);
	Hash::EngineHash withIndex(path, &indexed, &properties);
	const vector<uint32_t> read = withoutIndex.getProfileIdsForPropertyValue(
		hashAnyAlignmentIndexed[0],
		hashAnyAlignmentIndexedValues[0]);
	EXPECT_FALSE(read.empty());
	EXPECT_TRUE(read == withIndex.getProfileIdsForPropertyValue(
		hashAnyAlignmentIndexed[0],
		hashAnyAlignmentIndexedValues[0]));
}
