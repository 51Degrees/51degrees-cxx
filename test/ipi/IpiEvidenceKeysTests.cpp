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
 * @file IpiEvidenceKeysTests.cpp
 * @brief Tests the evidence keys an IP Intelligence data set accepts, as
 * fiftyoneDegreesIpiGetEvidenceKeys writes them and as EngineIpi::getKeys
 * returns them.
 *
 * The keys expected are built from the data set's unique headers, each
 * header under the query prefix and then the server prefix, keeping each key
 * where it first appears. Both the C function and the engine must give
 * exactly that list in that order, for each data file found.
 */

#include "../common/Base.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/ipi.h"

using std::string;
using std::vector;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

namespace {

/** The free data file holding the autonomous system properties. */
const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/**
 * The keys of the data set. For each unique header there is a key under
 * query. and then server., each kept where it first appears.
 */
vector<string> expectedKeys(fiftyoneDegreesDataSetIpi *dataSet) {
	static const char * const prefixes[] = { "query.", "server." };
	vector<string> keys;
	const fiftyoneDegreesHeaders *headers = dataSet->b.b.uniqueHeaders;
	const uint32_t count = headers == nullptr ? 0 : headers->count;
	for (uint32_t i = 0; i < count; i++) {
		for (const char *prefix : prefixes) {
			const string key = string(prefix) + headers->items[i].name;
			if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
				keys.push_back(key);
			}
		}
	}
	return keys;
}

/** Splits the text at each separator. */
vector<string> split(const string &text, const string &separator) {
	vector<string> keys;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find(separator, start);
		if (end == string::npos) {
			end = text.size();
		}
		keys.push_back(text.substr(start, end - start));
		start = end + separator.size();
	}
	return keys;
}

/** The keys as the C function writes them, joined by the separator. */
string writeKeys(
	fiftyoneDegreesDataSetIpi *dataSet,
	const char *separator) {
	const size_t length = fiftyoneDegreesIpiGetEvidenceKeys(
		dataSet,
		nullptr,
		0,
		separator);
	vector<char> buffer(length + 1, 'x');
	EXPECT_EQ(length, fiftyoneDegreesIpiGetEvidenceKeys(
		dataSet,
		buffer.data(),
		buffer.size(),
		separator)) << "The length needed changed once the buffer was given.";
	EXPECT_EQ(length, strlen(buffer.data())) << "The keys written were not "
		"the length given.";
	return string(buffer.data());
}

}

class IpiEvidenceKeysTests : public Base {
protected:
	/**
	 * Opens the data file with the C library. Returns false and records a
	 * failure if it cannot be opened.
	 */
	bool open(const string &path, fiftyoneDegreesResourceManager *manager) {
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiDefaultConfig;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				manager,
				&config,
				&properties,
				path.c_str(),
				exception);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status) <<
			"The data file '" << path << "' could not be opened.";
		EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		return status == FIFTYONE_DEGREES_STATUS_SUCCESS &&
			FIFTYONE_DEGREES_EXCEPTION_OKAY;
	}

	/**
	 * Checks the keys of the data file in the C library and in the engine.
	 * Skips if there is no such file.
	 */
	void verifyFile(const char *fileName) {
		const string path = GetFilePath(_dataFolderName, fileName);
		if (path.empty()) {
			GTEST_SKIP() << "The data file '" << fileName << "' was not found.";
		}
		fiftyoneDegreesResourceManager manager;
		if (open(path, &manager) == false) {
			return;
		}
		fiftyoneDegreesDataSetIpi *dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		const vector<string> expected = expectedKeys(dataSet);
		const string text = writeKeys(dataSet, ",");
		const vector<string> written = split(text, ",");
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);

		std::cout << "Evidence keys of '" << path << "': " << text << "\n";
		EXPECT_EQ(expected, written) << "The C library listed different "
			"keys for '" << path << "'.";

		// Each header gives a query key and then a server key.
		ASSERT_GT(written.size(), (size_t)0);
		ASSERT_EQ((size_t)0, written.size() % 2);
		for (size_t i = 0; i < written.size(); i += 2) {
			ASSERT_EQ(0, written[i].compare(0, strlen("query."), "query."));
			ASSERT_EQ(0, written[i + 1].compare(
				0, strlen("server."), "server."));
			EXPECT_EQ(
				written[i].substr(strlen("query.")),
				written[i + 1].substr(strlen("server.")));
		}

		// The engine gives the same keys from its own data set.
		ConfigIpi config;
		RequiredPropertiesConfig properties;
		EngineIpi engine(path, &config, &properties);
		EXPECT_EQ(expected, *engine.getKeys()) << "The engine listed "
			"different keys for '" << path << "'.";
	}

	/** The path of the first data file found, or empty if there is none. */
	string findDataFile() {
		string path = GetFilePath(_dataFolderName, asnFileName);
		for (int i = _IpiFileNamesLength - 1; i >= 0 && path.empty(); i--) {
			path = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		return path;
	}
};

TEST_F(IpiEvidenceKeysTests, LiteDataFile) {
	verifyFile("51Degrees-LiteV41.ipi");
}

TEST_F(IpiEvidenceKeysTests, EnterpriseDataFile) {
	verifyFile("51Degrees-EnterpriseIpiV41.ipi");
}

TEST_F(IpiEvidenceKeysTests, AsnDataFile) {
	verifyFile(asnFileName);
}

/**
 * The length returned is the same however long the buffer is, and a buffer
 * that is too short holds as much of the keys as fits.
 */
TEST_F(IpiEvidenceKeysTests, BufferLength) {
	const string path = findDataFile();
	if (path.empty()) {
		GTEST_SKIP() << "No IP Intelligence data file was found.";
	}
	fiftyoneDegreesResourceManager manager;
	ASSERT_TRUE(open(path, &manager));
	fiftyoneDegreesDataSetIpi *dataSet =
		fiftyoneDegreesDataSetIpiGet(&manager);
	const string all = writeKeys(dataSet, ",");
	const size_t length = all.size();

	// A NULL buffer is read as having no length.
	const size_t withNull = fiftyoneDegreesIpiGetEvidenceKeys(
		dataSet,
		nullptr,
		100,
		",");

	// One character short of the null terminator.
	vector<char> shortBuffer(length, 'x');
	const size_t withShort = fiftyoneDegreesIpiGetEvidenceKeys(
		dataSet,
		shortBuffer.data(),
		shortBuffer.size(),
		",");

	// A buffer of no length is not written to.
	char untouched = 'x';
	const size_t withEmpty = fiftyoneDegreesIpiGetEvidenceKeys(
		dataSet,
		&untouched,
		0,
		",");

	// A longer separator, and none at all.
	const string spaced = writeKeys(dataSet, " | ");
	const string joined = writeKeys(dataSet, "");

	fiftyoneDegreesDataSetIpiRelease(dataSet);
	fiftyoneDegreesResourceManagerFree(&manager);

	const vector<string> keys = split(all, ",");
	ASSERT_GT(keys.size(), (size_t)1);
	EXPECT_EQ(length, withNull);
	EXPECT_EQ(length, withShort);
	EXPECT_EQ(all.substr(0, length - 1), string(shortBuffer.data()));
	EXPECT_EQ(length, withEmpty);
	EXPECT_EQ('x', untouched);
	EXPECT_EQ(keys, split(spaced, " | "));
	EXPECT_EQ(length + 2 * (keys.size() - 1), spaced.size());
	string together;
	for (const string &key : keys) {
		together.append(key);
	}
	EXPECT_EQ(together, joined);
}
