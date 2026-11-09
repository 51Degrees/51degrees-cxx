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
 * @file HashEvidenceKeysTests.cpp
 * @brief Tests the evidence keys a Hash data set accepts, as
 * fiftyoneDegreesHashGetEvidenceKeys writes them and as EngineHash::getKeys
 * returns them.
 *
 * The keys expected are built from the data set's unique headers and
 * overridable properties, each with its prefixes, and then the keys every
 * data set accepts, keeping each key where it first appears. Both the C
 * function and the engine must give exactly that list in that order, for
 * each data file found and with and without upper prefixed headers.
 *
 * The Lite and TAC data files must also give the keys written out below,
 * word for word, so a change to their keys fails a test even when it follows
 * the rule above.
 */

#include "../../common/pch.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"

using std::string;
using std::vector;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

/** The keys every Hash data set accepts, after those from the data file. */
const char * const fixedKeys[] = {
	"query.51D_deviceId",
	"query.51D_gethighentropyvalues",
	"query.51D_structureduseragent",
	"cookie.51D_gethighentropyvalues",
	"cookie.51D_structureduseragent",
	"query.51D_ProfileIds",
	"cookie.51D_ProfileIds"
};

const size_t fixedKeysCount = sizeof(fixedKeys) / sizeof(fixedKeys[0]);

/*
 * The keys of the Lite and TAC data files, word for word and in order, with
 * upper prefixed headers off and then on. Each list is written out rather
 * than worked out from the data set, so a test fails when the keys change. A
 * data file that adds or removes a header or an overridable property changes
 * its list, which is then updated here.
 */

const char * const liteKeysOff[] = {
	"header.Device-Stock-UA",
	"query.Device-Stock-UA",
	"header.X-Device-User-Agent",
	"query.X-Device-User-Agent",
	"header.X-OperaMini-Phone-UA",
	"query.X-OperaMini-Phone-UA",
	"header.X-UCBrowser-Device-UA",
	"query.X-UCBrowser-Device-UA",
	"header.User-Agent",
	"query.User-Agent",
	"header.Sec-CH-UA-Mobile",
	"query.Sec-CH-UA-Mobile",
	"header.Sec-CH-UA-Platform",
	"query.Sec-CH-UA-Platform",
	"header.Sec-CH-UA-Model",
	"query.Sec-CH-UA-Model",
	"header.Sec-CH-UA-Platform-Version",
	"query.Sec-CH-UA-Platform-Version",
	"header.Sec-CH-UA",
	"query.Sec-CH-UA",
	"header.Sec-CH-UA-Full-Version-List",
	"query.Sec-CH-UA-Full-Version-List",
	"cookie.51D_JavascriptGetHighEntropyValues",
	"query.51D_JavascriptGetHighEntropyValues",
	"cookie.51D_JavascriptHardwareProfile",
	"query.51D_JavascriptHardwareProfile",
	"query.51D_deviceId",
	"query.51D_gethighentropyvalues",
	"query.51D_structureduseragent",
	"cookie.51D_gethighentropyvalues",
	"cookie.51D_structureduseragent",
	"query.51D_ProfileIds",
	"cookie.51D_ProfileIds"
};

const char * const liteKeysOn[] = {
	"header.HTTP_Device-Stock-UA",
	"query.HTTP_Device-Stock-UA",
	"header.HTTP_X-Device-User-Agent",
	"query.HTTP_X-Device-User-Agent",
	"header.HTTP_X-OperaMini-Phone-UA",
	"query.HTTP_X-OperaMini-Phone-UA",
	"header.HTTP_X-UCBrowser-Device-UA",
	"query.HTTP_X-UCBrowser-Device-UA",
	"header.HTTP_User-Agent",
	"query.HTTP_User-Agent",
	"header.HTTP_Sec-CH-UA-Mobile",
	"query.HTTP_Sec-CH-UA-Mobile",
	"header.HTTP_Sec-CH-UA-Platform",
	"query.HTTP_Sec-CH-UA-Platform",
	"header.HTTP_Sec-CH-UA-Model",
	"query.HTTP_Sec-CH-UA-Model",
	"header.HTTP_Sec-CH-UA-Platform-Version",
	"query.HTTP_Sec-CH-UA-Platform-Version",
	"header.HTTP_Sec-CH-UA",
	"query.HTTP_Sec-CH-UA",
	"header.HTTP_Sec-CH-UA-Full-Version-List",
	"query.HTTP_Sec-CH-UA-Full-Version-List",
	"cookie.51D_JavascriptGetHighEntropyValues",
	"query.51D_JavascriptGetHighEntropyValues",
	"cookie.51D_JavascriptHardwareProfile",
	"query.51D_JavascriptHardwareProfile",
	"query.51D_deviceId",
	"query.51D_gethighentropyvalues",
	"query.51D_structureduseragent",
	"cookie.51D_gethighentropyvalues",
	"cookie.51D_structureduseragent",
	"query.51D_ProfileIds",
	"cookie.51D_ProfileIds"
};

const char * const tacKeysOff[] = {
	"header.Device-Stock-UA",
	"query.Device-Stock-UA",
	"header.X-Device-User-Agent",
	"query.X-Device-User-Agent",
	"header.X-OperaMini-Phone-UA",
	"query.X-OperaMini-Phone-UA",
	"header.X-UCBrowser-Device-UA",
	"query.X-UCBrowser-Device-UA",
	"header.User-Agent",
	"query.User-Agent",
	"header.Sec-CH-UA-Mobile",
	"query.Sec-CH-UA-Mobile",
	"header.Sec-CH-UA-Platform",
	"query.Sec-CH-UA-Platform",
	"header.Sec-CH-UA-Model",
	"query.Sec-CH-UA-Model",
	"header.Sec-CH-UA-Platform-Version",
	"query.Sec-CH-UA-Platform-Version",
	"header.Sec-CH-UA",
	"query.Sec-CH-UA",
	"header.Sec-CH-UA-Full-Version-List",
	"query.Sec-CH-UA-Full-Version-List",
	"cookie.51D_HasWebDriver",
	"query.51D_HasWebDriver",
	"cookie.51D_HasWebDriverJavaScript",
	"query.51D_HasWebDriverJavaScript",
	"cookie.51D_IsVerifiediPhone",
	"query.51D_IsVerifiediPhone",
	"cookie.51D_IsVerifiediPhoneJavaScript",
	"query.51D_IsVerifiediPhoneJavaScript",
	"cookie.51D_IsVisible",
	"query.51D_IsVisible",
	"cookie.51D_IsVisibleJavaScript",
	"query.51D_IsVisibleJavaScript",
	"cookie.51D_Javascript",
	"query.51D_Javascript",
	"cookie.51D_JavascriptBandwidth",
	"query.51D_JavascriptBandwidth",
	"cookie.51D_JavaScriptBrowserOverride",
	"query.51D_JavaScriptBrowserOverride",
	"cookie.51D_JavascriptGetHighEntropyValues",
	"query.51D_JavascriptGetHighEntropyValues",
	"cookie.51D_JavascriptHardwareProfile",
	"query.51D_JavascriptHardwareProfile",
	"cookie.51D_JavascriptImageOptimiser",
	"query.51D_JavascriptImageOptimiser",
	"cookie.51D_PixelRatio",
	"query.51D_PixelRatio",
	"cookie.51D_PixelRatioJavascript",
	"query.51D_PixelRatioJavascript",
	"cookie.51D_ScreenPixelsHeight",
	"query.51D_ScreenPixelsHeight",
	"cookie.51D_ScreenPixelsHeightJavaScript",
	"query.51D_ScreenPixelsHeightJavaScript",
	"cookie.51D_ScreenPixelsWidth",
	"query.51D_ScreenPixelsWidth",
	"cookie.51D_ScreenPixelsWidthJavaScript",
	"query.51D_ScreenPixelsWidthJavaScript",
	"cookie.51D_ThirdPartyCookiesEnabled",
	"query.51D_ThirdPartyCookiesEnabled",
	"cookie.51D_ThirdPartyCookiesEnabledJavaScript",
	"query.51D_ThirdPartyCookiesEnabledJavaScript",
	"query.51D_deviceId",
	"query.51D_gethighentropyvalues",
	"query.51D_structureduseragent",
	"cookie.51D_gethighentropyvalues",
	"cookie.51D_structureduseragent",
	"query.51D_ProfileIds",
	"cookie.51D_ProfileIds"
};

const char * const tacKeysOn[] = {
	"header.HTTP_Device-Stock-UA",
	"query.HTTP_Device-Stock-UA",
	"header.HTTP_X-Device-User-Agent",
	"query.HTTP_X-Device-User-Agent",
	"header.HTTP_X-OperaMini-Phone-UA",
	"query.HTTP_X-OperaMini-Phone-UA",
	"header.HTTP_X-UCBrowser-Device-UA",
	"query.HTTP_X-UCBrowser-Device-UA",
	"header.HTTP_User-Agent",
	"query.HTTP_User-Agent",
	"header.HTTP_Sec-CH-UA-Mobile",
	"query.HTTP_Sec-CH-UA-Mobile",
	"header.HTTP_Sec-CH-UA-Platform",
	"query.HTTP_Sec-CH-UA-Platform",
	"header.HTTP_Sec-CH-UA-Model",
	"query.HTTP_Sec-CH-UA-Model",
	"header.HTTP_Sec-CH-UA-Platform-Version",
	"query.HTTP_Sec-CH-UA-Platform-Version",
	"header.HTTP_Sec-CH-UA",
	"query.HTTP_Sec-CH-UA",
	"header.HTTP_Sec-CH-UA-Full-Version-List",
	"query.HTTP_Sec-CH-UA-Full-Version-List",
	"cookie.51D_HasWebDriver",
	"query.51D_HasWebDriver",
	"cookie.51D_HasWebDriverJavaScript",
	"query.51D_HasWebDriverJavaScript",
	"cookie.51D_IsVerifiediPhone",
	"query.51D_IsVerifiediPhone",
	"cookie.51D_IsVerifiediPhoneJavaScript",
	"query.51D_IsVerifiediPhoneJavaScript",
	"cookie.51D_IsVisible",
	"query.51D_IsVisible",
	"cookie.51D_IsVisibleJavaScript",
	"query.51D_IsVisibleJavaScript",
	"cookie.51D_Javascript",
	"query.51D_Javascript",
	"cookie.51D_JavascriptBandwidth",
	"query.51D_JavascriptBandwidth",
	"cookie.51D_JavaScriptBrowserOverride",
	"query.51D_JavaScriptBrowserOverride",
	"cookie.51D_JavascriptGetHighEntropyValues",
	"query.51D_JavascriptGetHighEntropyValues",
	"cookie.51D_JavascriptHardwareProfile",
	"query.51D_JavascriptHardwareProfile",
	"cookie.51D_JavascriptImageOptimiser",
	"query.51D_JavascriptImageOptimiser",
	"cookie.51D_PixelRatio",
	"query.51D_PixelRatio",
	"cookie.51D_PixelRatioJavascript",
	"query.51D_PixelRatioJavascript",
	"cookie.51D_ScreenPixelsHeight",
	"query.51D_ScreenPixelsHeight",
	"cookie.51D_ScreenPixelsHeightJavaScript",
	"query.51D_ScreenPixelsHeightJavaScript",
	"cookie.51D_ScreenPixelsWidth",
	"query.51D_ScreenPixelsWidth",
	"cookie.51D_ScreenPixelsWidthJavaScript",
	"query.51D_ScreenPixelsWidthJavaScript",
	"cookie.51D_ThirdPartyCookiesEnabled",
	"query.51D_ThirdPartyCookiesEnabled",
	"cookie.51D_ThirdPartyCookiesEnabledJavaScript",
	"query.51D_ThirdPartyCookiesEnabledJavaScript",
	"query.51D_deviceId",
	"query.51D_gethighentropyvalues",
	"query.51D_structureduseragent",
	"cookie.51D_gethighentropyvalues",
	"cookie.51D_structureduseragent",
	"query.51D_ProfileIds",
	"cookie.51D_ProfileIds"
};

/** The keys of the array, in order. */
template <size_t N>
vector<string> listOf(const char * const (&keys)[N]) {
	return vector<string>(keys, keys + N);
}

/** Adds the key unless the list already holds it. */
void addKey(vector<string> &keys, const string &key) {
	if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
		keys.push_back(key);
	}
}

/**
 * The keys of the data set. For each unique header that is not a pseudo
 * header there is a key under header. and then query., with HTTP_ before the
 * name when the configuration expects it. For each overridable property
 * there is a key under cookie. and then query., with 51D_ before the name
 * when the overrides need it. The fixed keys follow.
 */
vector<string> expectedKeys(fiftyoneDegreesDataSetHash *dataSet) {
	static const char * const headerPrefixes[] = { "header.", "query." };
	static const char * const overridePrefixes[] = { "cookie.", "query." };
	vector<string> keys;
	const fiftyoneDegreesHeaders *headers = dataSet->b.b.uniqueHeaders;
	const string upper =
		dataSet->config.b.b.usesUpperPrefixedHeaders ? "HTTP_" : "";
	for (uint32_t i = 0; i < headers->count; i++) {
		const char *name = headers->items[i].name;
		if (strstr(name, "\x1F") == nullptr) {
			for (const char *prefix : headerPrefixes) {
				addKey(keys, prefix + upper + name);
			}
		}
	}
	const fiftyoneDegreesOverridePropertyArray *overrides =
		dataSet->b.b.overridable;
	if (overrides != nullptr) {
		const string infix = overrides->prefix ? "51D_" : "";
		for (uint32_t i = 0; i < overrides->count; i++) {
			const char *name = FIFTYONE_DEGREES_STRING(
				overrides->items[i].available->name.data.ptr);
			for (const char *prefix : overridePrefixes) {
				addKey(keys, prefix + infix + name);
			}
		}
	}
	for (const char *key : fixedKeys) {
		addKey(keys, key);
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
	fiftyoneDegreesDataSetHash *dataSet,
	const char *separator) {
	const size_t length = fiftyoneDegreesHashGetEvidenceKeys(
		dataSet,
		nullptr,
		0,
		separator);
	vector<char> buffer(length + 1, 'x');
	EXPECT_EQ(length, fiftyoneDegreesHashGetEvidenceKeys(
		dataSet,
		buffer.data(),
		buffer.size(),
		separator)) << "The length needed changed once the buffer was given.";
	EXPECT_EQ(length, strlen(buffer.data())) << "The keys written were not "
		"the length given.";
	return string(buffer.data());
}

}

class HashEvidenceKeysTests : public Base {
protected:
	/**
	 * Opens the data file with the C library, with or without upper prefixed
	 * headers. Returns false and records a failure if it cannot be opened.
	 */
	bool open(
		const string &path,
		bool upper,
		fiftyoneDegreesResourceManager *manager) {
		fiftyoneDegreesConfigHash config = fiftyoneDegreesHashDefaultConfig;
		config.b.b.usesUpperPrefixedHeaders = upper;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromFile(
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
	 * Checks the keys of the data file in the C library and in the engine,
	 * with and without upper prefixed headers. Skips if there is no such file.
	 */
	void verifyFile(const char *fileName) {
		const string path = GetFilePath(_dataFolderName, fileName);
		if (path.empty()) {
			GTEST_SKIP() << "The data file '" << fileName << "' was not found.";
		}
		verifyKeys(path, false);
		verifyKeys(path, true);
	}

	void verifyKeys(const string &path, bool upper) {
		fiftyoneDegreesResourceManager manager;
		if (open(path, upper, &manager) == false) {
			return;
		}
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(&manager);
		const vector<string> expected = expectedKeys(dataSet);
		const string text = writeKeys(dataSet, ",");
		const vector<string> written = split(text, ",");
		fiftyoneDegreesDataSetHashRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);

		std::cout << "Evidence keys of '" << path << "' with upper prefixed "
			"headers " << (upper ? "on" : "off") << ": " << text << "\n";
		EXPECT_EQ(expected, written) << "The C library listed different "
			"keys for '" << path << "'.";

		// The data file always names a header, and the fixed keys come last.
		ASSERT_GT(written.size(), fixedKeysCount);
		const size_t first = written.size() - fixedKeysCount;
		for (size_t i = 0; i < fixedKeysCount; i++) {
			EXPECT_EQ(string(fixedKeys[i]), written[first + i]);
		}
		for (const string &key : written) {
			if (key.compare(0, strlen("header."), "header.") == 0) {
				EXPECT_EQ(upper, key.compare(
					strlen("header."),
					strlen("HTTP_"),
					"HTTP_") == 0) << "The key '" << key << "' does not "
					"follow the configuration's upper prefixed headers.";
			}
		}

		// The engine gives the same keys from its own data set.
		ConfigHash config;
		config.setUseUpperPrefixHeaders(upper);
		RequiredPropertiesConfig properties;
		EngineHash engine(path, &config, &properties);
		EXPECT_EQ(expected, *engine.getKeys()) << "The engine listed "
			"different keys for '" << path << "'.";
	}

	/**
	 * Checks that the C library and the engine give exactly the keys listed
	 * for the data file, with upper prefixed headers off and then on. Skips
	 * if there is no such file.
	 */
	void verifyListed(
		const char *fileName,
		const vector<string> &off,
		const vector<string> &on) {
		const string path = GetFilePath(_dataFolderName, fileName);
		if (path.empty()) {
			GTEST_SKIP() << "The data file '" << fileName << "' was not found.";
		}
		verifyListedKeys(path, false, off);
		verifyListedKeys(path, true, on);
	}

	void verifyListedKeys(
		const string &path,
		bool upper,
		const vector<string> &listed) {
		const string where = "'" + path + "' with upper prefixed headers " +
			(upper ? "on" : "off") + ". If the data file has changed, update "
			"the keys listed in this test.";
		fiftyoneDegreesResourceManager manager;
		if (open(path, upper, &manager) == false) {
			return;
		}
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(&manager);
		const vector<string> written = split(writeKeys(dataSet, ","), ",");
		fiftyoneDegreesDataSetHashRelease(dataSet);
		fiftyoneDegreesResourceManagerFree(&manager);
		EXPECT_EQ(listed, written) << "The C library gave other keys for " <<
			where;

		ConfigHash config;
		config.setUseUpperPrefixHeaders(upper);
		RequiredPropertiesConfig properties;
		EngineHash engine(path, &config, &properties);
		EXPECT_EQ(listed, *engine.getKeys()) << "The engine gave other keys "
			"for " << where;
	}

	/** The path of the first data file found, or empty if there is none. */
	string findDataFile() {
		string path;
		for (int i = _HashFileNamesLength - 1; i >= 0 && path.empty(); i--) {
			path = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		return path;
	}
};

TEST_F(HashEvidenceKeysTests, LiteDataFile) {
	verifyFile("51Degrees-LiteV4.1.hash");
}

TEST_F(HashEvidenceKeysTests, EnterpriseDataFile) {
	verifyFile("51Degrees-EnterpriseV4.1.hash");
}

TEST_F(HashEvidenceKeysTests, TacDataFile) {
	verifyFile("TAC-HashV41.hash");
}

TEST_F(HashEvidenceKeysTests, LiteDataFile_GivesTheListedKeys) {
	verifyListed(
		"51Degrees-LiteV4.1.hash",
		listOf(liteKeysOff),
		listOf(liteKeysOn));
}

TEST_F(HashEvidenceKeysTests, TacDataFile_GivesTheListedKeys) {
	verifyListed(
		"TAC-HashV41.hash",
		listOf(tacKeysOff),
		listOf(tacKeysOn));
}

/**
 * The length returned is the same however long the buffer is, and a buffer
 * that is too short holds as much of the keys as fits.
 */
TEST_F(HashEvidenceKeysTests, BufferLength) {
	const string path = findDataFile();
	ASSERT_FALSE(path.empty()) << "No Hash data file was found.";
	fiftyoneDegreesResourceManager manager;
	ASSERT_TRUE(open(path, false, &manager));
	fiftyoneDegreesDataSetHash *dataSet =
		fiftyoneDegreesDataSetHashGet(&manager);
	const string all = writeKeys(dataSet, ",");
	const size_t length = all.size();

	// A NULL buffer is read as having no length.
	const size_t withNull = fiftyoneDegreesHashGetEvidenceKeys(
		dataSet,
		nullptr,
		100,
		",");

	// One character short of the null terminator.
	vector<char> shortBuffer(length, 'x');
	const size_t withShort = fiftyoneDegreesHashGetEvidenceKeys(
		dataSet,
		shortBuffer.data(),
		shortBuffer.size(),
		",");

	// A buffer of no length is not written to.
	char untouched = 'x';
	const size_t withEmpty = fiftyoneDegreesHashGetEvidenceKeys(
		dataSet,
		&untouched,
		0,
		",");

	// A longer separator, and none at all.
	const string spaced = writeKeys(dataSet, " | ");
	const string joined = writeKeys(dataSet, "");

	fiftyoneDegreesDataSetHashRelease(dataSet);
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
