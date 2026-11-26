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
 * @file IpiAnyAlignmentTests.cpp
 * @brief Reads the free ASN data file from memory placed at each byte offset
 * from 0 to 7 of a buffer, so every packed record of the file starts at each
 * alignment in turn, and checks that each offset gives the same answers.
 *
 * The data file packs its records with no padding, so a number in one can
 * start at any byte. Built with the alignment check of the undefined
 * behavior sanitizer, a read that takes such a number to be aligned is
 * reported.
 *
 * Each offset reads the data set header, the component key value pairs, the
 * values, the profile offsets and the value indexes of the profiles, which
 * the C++ metadata reads.
 */

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/MetaDataIpi.hpp"
#include "../../src/ipi/ipi.h"

namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

using std::string;
using std::unique_ptr;
using std::vector;
using Values =
	Common::Collection<Common::ValueMetaDataKey, Common::ValueMetaData>;
using Profiles = Common::Collection<uint32_t, Common::ProfileMetaData>;

namespace {

/** The free ASN data file, which the data repository's script downloads. */
const char ipiAnyAlignmentFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** The byte offsets of the buffer the data file is placed at. */
const size_t ipiAnyAlignmentOffsets = 8;

/** The profiles whose values the C++ metadata reads, spread across them. */
const uint32_t ipiAnyAlignmentProfiles = 100;

/** The size of the buffer each value string is written to. */
const size_t ipiAnyAlignmentTextBytes = 64 * 1024;

/** IP addresses in different ranges of both versions. */
const char *ipiAnyAlignmentAddresses[] = {
	"185.28.167.77",
	"8.8.8.8",
	"1.1.1.1",
	"81.2.69.142",
	"0.0.0.0",
	"255.255.255.255",
	"2001:4860:4860::8888",
	"2a01:111:f400::1",
	"::1",
	"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
};

/** Frees the manager, and with it the data set, at the end of a scope. */
std::shared_ptr<fiftyoneDegreesResourceManager> ipiAnyAlignmentManager() {
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
void ipiAnyAlignmentRead(const string &path, vector<unsigned char> &data) {
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

/** Adds the values of every property for each IP address. */
void ipiAnyAlignmentResults(
	fiftyoneDegreesResourceManager *manager,
	std::stringstream &answers) {
	vector<char> buffer(ipiAnyAlignmentTextBytes);
	fiftyoneDegreesResultsIpi *results =
		fiftyoneDegreesResultsIpiCreate(manager);
	ASSERT_NE(nullptr, results);
	const fiftyoneDegreesDataSetIpi *dataSet =
		(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
	const int count = (int)dataSet->b.b.available->count;
	EXPECT_LT(0, count);
	for (const char *address : ipiAnyAlignmentAddresses) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			address,
			strlen(address),
			exception);
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << address;
		for (int i = 0; i < count; i++) {
			buffer[0] = '\0';
			fiftyoneDegreesResultsIpiGetValuesStringByRequiredPropertyIndex(
				results,
				i,
				buffer.data(),
				buffer.size(),
				"|",
				exception);
			EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << address;
			answers << buffer.data() << "\n";
		}
	}
	fiftyoneDegreesResultsIpiFree(results);
}

/**
 * Adds the values of a sample of the profiles, read by position and found
 * again by key through the C++ metadata.
 */
void ipiAnyAlignmentProfileValues(
	std::shared_ptr<fiftyoneDegreesResourceManager> manager,
	std::stringstream &answers) {
	Ipi::MetaDataIpi metaData(manager);
	unique_ptr<Profiles> profiles(metaData.getProfiles());
	const uint32_t count = profiles->getSize();
	ASSERT_LT(0u, count);
	const uint32_t step = count / ipiAnyAlignmentProfiles + 1;
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
void ipiAnyAlignmentAnswers(
	const vector<unsigned char> &file,
	size_t offset,
	string &answers) {
	vector<unsigned char> buffer(file.size() + ipiAnyAlignmentOffsets);
	memcpy(buffer.data() + offset, file.data(), file.size());
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
	config.b.freeData = false;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	std::shared_ptr<fiftyoneDegreesResourceManager> manager =
		ipiAnyAlignmentManager();
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiInitManagerFromMemory(
			manager.get(),
			&config,
			&properties,
			buffer.data() + offset,
			(fiftyoneDegreesFileOffset)file.size(),
			exception)) << "Offset " << offset;
	ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) << "Offset " << offset;
	std::stringstream found;
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentResults(manager.get(), found));
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentProfileValues(manager, found));
	answers = found.str();
}

}

class IpiAnyAlignmentTests : public Base {
};

/**
 * The data file placed at each offset of a buffer gives the same values and
 * profile values as at the start of the buffer.
 */
TEST_F(IpiAnyAlignmentTests, MemoryAtEveryOffset) {
	const string path = GetFilePath(_dataFolderName, ipiAnyAlignmentFileName);
	if (path.empty()) {
		GTEST_SKIP() << "The file '" << ipiAnyAlignmentFileName << "' was not "
			"found. Run data/get-data-files.sh or .ps1 to download it.";
	}
	vector<unsigned char> file;
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentRead(path, file));
	string expected;
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentAnswers(file, 0, expected));
	ASSERT_FALSE(expected.empty());
	for (size_t offset = 1; offset < ipiAnyAlignmentOffsets; offset++) {
		string answers;
		ASSERT_NO_FATAL_FAILURE(
			ipiAnyAlignmentAnswers(file, offset, answers));
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
TEST_F(IpiAnyAlignmentTests, FileMatchesMemory) {
	const string path = GetFilePath(_dataFolderName, ipiAnyAlignmentFileName);
	if (path.empty()) {
		GTEST_SKIP() << "The file '" << ipiAnyAlignmentFileName << "' was not "
			"found. Run data/get-data-files.sh or .ps1 to download it.";
	}
	vector<unsigned char> file;
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentRead(path, file));
	string expected;
	{
		std::stringstream found;
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
		config.b.freeData = false;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		std::shared_ptr<fiftyoneDegreesResourceManager> manager =
			ipiAnyAlignmentManager();
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiInitManagerFromMemory(
				manager.get(),
				&config,
				&properties,
				file.data(),
				(fiftyoneDegreesFileOffset)file.size(),
				exception));
		ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentResults(manager.get(), found));
		expected = found.str();
	}
	std::stringstream found;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	std::shared_ptr<fiftyoneDegreesResourceManager> manager =
		ipiAnyAlignmentManager();
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiInitManagerFromFile(
			manager.get(),
			&config,
			&properties,
			path.c_str(),
			exception));
	ASSERT_NO_FATAL_FAILURE(ipiAnyAlignmentResults(manager.get(), found));
	EXPECT_TRUE(expected == found.str());
}
