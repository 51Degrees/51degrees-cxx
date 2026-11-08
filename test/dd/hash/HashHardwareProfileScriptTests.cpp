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
 * @file HashHardwareProfileScriptTests.cpp
 * @brief Tests of the script written for a value of the
 * JavascriptHardwareProfile property. The C function is checked with every
 * length of buffer up to one that fits. ResultsHash is checked to give, for
 * each value the C results hold for the same evidence, that value inside the
 * text given here, for every User-Agent in the test file and every Hash data
 * file found, so a change to the script fails the test.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/common/textfile.h"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "../../../src/dd/hash/hash.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {

/** The script starts with this, before the value. */
const char scriptStart[] = "var profileIds = []\n";

/** The script ends with this, after the value. */
const char scriptEnd[] =
	"\ndocument.cookie = \"51D_ProfileIds=\" + profileIds.join(\"|\")";

/** A body that pushes a profile id, to stand for a value of the property. */
const char body[] =
	"if (navigator.platform == \"iPhone\") { profileIds.push(\"12280\"); }";

const char propertyName[] = "JavascriptHardwareProfile";

/** An iPhone, so that the evidence includes Apple hardware. */
const char iPhoneUserAgent[] =
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53";

/** Bytes after the buffer given, which the function must not write. */
const size_t guardLength = 8;

const char guardByte = '#';

std::string expectedScript(const std::string &value) {
	return std::string(scriptStart) + value + scriptEnd;
}

/** The script the C function writes, into a buffer that fits it. */
std::string scriptOf(const char *value) {
	const size_t length = fiftyoneDegreesHashGetHardwareProfileScript(
		value,
		nullptr,
		0);
	std::vector<char> buffer(length + 1, guardByte);
	EXPECT_EQ(
		length,
		fiftyoneDegreesHashGetHardwareProfileScript(
			value,
			buffer.data(),
			buffer.size()));
	EXPECT_EQ('\0', buffer[length]);
	return std::string(buffer.data(), length);
}

/** An engine that gives its C manager, to make C results from its data. */
class ManagerEngine : public EngineHash {
public:
	using EngineHash::EngineHash;
	fiftyoneDegreesResourceManager* getManager() const {
		return manager.get();
	}
};

/**
 * Sets the values of the property that C results from the User-Agent hold,
 * and returns whether they hold any.
 */
bool valuesFromC(
	fiftyoneDegreesResourceManager *manager,
	const char *userAgent,
	int requiredPropertyIndex,
	std::vector<std::string> &values) {
	bool hasValues = false;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	fiftyoneDegreesResultsHash *results =
		fiftyoneDegreesResultsHashCreate(manager, 0, 0);
	fiftyoneDegreesResultsHashFromUserAgent(
		results,
		userAgent,
		strlen(userAgent),
		exception);
	if (FIFTYONE_DEGREES_EXCEPTION_OKAY) {
		hasValues = fiftyoneDegreesResultsHashGetHasValues(
			results,
			requiredPropertyIndex,
			exception);
	}
	if (hasValues && FIFTYONE_DEGREES_EXCEPTION_OKAY) {
		fiftyoneDegreesCollectionItem *items =
			fiftyoneDegreesResultsHashGetValues(
				results,
				requiredPropertyIndex,
				exception);
		for (uint32_t i = 0;
			items != nullptr && i < results->values.count;
			i++) {
			const char *value = FIFTYONE_DEGREES_STRING(items[i].data.ptr);
			if (value != nullptr) {
				values.push_back(value);
			}
		}
	}
	fiftyoneDegreesResultsHashFree(results);
	FIFTYONE_DEGREES_EXCEPTION_THROW;
	return hasValues;
}

void addLine(const char *line, void *state) {
	((std::vector<std::string>*)state)->push_back(line);
}

}

TEST(HashHardwareProfileScriptCTests, Script_WrapsTheValue) {
	EXPECT_EQ(expectedScript(body), scriptOf(body));
}

TEST(HashHardwareProfileScriptCTests, Script_WrapsAnEmptyValue) {
	EXPECT_EQ(expectedScript(""), scriptOf(""));
}

TEST(HashHardwareProfileScriptCTests, Script_TakesNullAsAnEmptyValue) {
	EXPECT_EQ(expectedScript(""), scriptOf(nullptr));
}

/**
 * Without a buffer the length is given whatever length is passed, as the
 * buffer is not there to write to.
 */
TEST(HashHardwareProfileScriptCTests, Length_IsGivenWithoutABuffer) {
	const size_t expected = expectedScript(body).size();
	EXPECT_EQ(
		expected,
		fiftyoneDegreesHashGetHardwareProfileScript(body, nullptr, 0));
	EXPECT_EQ(
		expected,
		fiftyoneDegreesHashGetHardwareProfileScript(body, nullptr, 1000));
}

/**
 * A buffer of every length from none to one that fits holds as much of the
 * script as fits and a null terminator, the full length is always given, and
 * nothing is written past the length passed.
 */
TEST(HashHardwareProfileScriptCTests, Buffer_HoldsWhatFitsAndNoMore) {
	const std::string expected = expectedScript(body);
	for (size_t length = 0; length <= expected.size() + 1; length++) {
		std::vector<char> buffer(length + guardLength, guardByte);
		EXPECT_EQ(
			expected.size(),
			fiftyoneDegreesHashGetHardwareProfileScript(
				body,
				buffer.data(),
				length)) << "length " << length;
		if (length > 0) {
			const std::string written = expected.substr(0, length - 1);
			EXPECT_EQ(
				written,
				std::string(buffer.data(), written.size())) <<
				"length " << length;
			EXPECT_EQ('\0', buffer[written.size()]) << "length " << length;
		}
		for (size_t i = length; i < buffer.size(); i++) {
			EXPECT_EQ(guardByte, buffer[i]) <<
				"length " << length << ", byte " << i;
		}
	}
}

/**
 * Tests through ResultsHash on every Hash data file found. A skip in SetUp
 * does not stop the test with the Visual Studio googletest, so each test
 * first checks that SetUp found a data file.
 */
class HashHardwareProfileScriptTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0; i < _HashFileNamesLength; i++) {
			std::string path = GetFilePath(
				_dataFolderName,
				_HashFileNames[i]);
			if (path.empty() == false) {
				dataFilePaths.push_back(path);
			}
		}
		if (dataFilePaths.empty()) {
			GTEST_SKIP() << "No Hash data file was found.";
		}
		userAgentsPath = GetFilePath(_dataFolderName, _userAgentsFileName);
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as a leak.
		std::vector<std::string>().swap(dataFilePaths);
		std::string().swap(userAgentsPath);
		Base::TearDown();
	}

protected:
	std::vector<std::string> dataFilePaths;
	std::string userAgentsPath;
};

/**
 * For each User-Agent, ResultsHash has a value of the property exactly when
 * the C results do, and its values are the C values, each inside the
 * script. Empty values are counted, and a paid data file must give at least
 * one script.
 */
TEST_F(HashHardwareProfileScriptTests, Results_GiveEachValueInTheScript) {
	if (dataFilePaths.empty()) {
		GTEST_SKIP() << "The set up did not finish.";
	}
	std::vector<std::string> userAgents = { iPhoneUserAgent };
	if (userAgentsPath.empty() == false) {
		char line[1000] = "";
		fiftyoneDegreesTextFileIterate(
			userAgentsPath.c_str(),
			line,
			(int)sizeof(line),
			&userAgents,
			addLine);
	}
	for (const std::string &path : dataFilePaths) {
		ConfigHash config;
		RequiredPropertiesConfig properties(propertyName);
		ManagerEngine engine(path.c_str(), &config, &properties);
		fiftyoneDegreesDataSetHash *dataSet =
			fiftyoneDegreesDataSetHashGet(engine.getManager());
		const int requiredPropertyIndex =
			fiftyoneDegreesPropertiesGetRequiredPropertyIndexFromName(
				dataSet->b.b.available,
				propertyName);
		fiftyoneDegreesDataSetHashRelease(dataSet);
		if (requiredPropertyIndex < 0) {
			printf("%s has no %s\n", path.c_str(), propertyName);
			continue;
		}
		size_t scripts = 0, emptyValues = 0, mostValues = 0;
		std::set<std::string> distinct;
		for (const std::string &userAgent : userAgents) {
			std::vector<std::string> values, expected;
			const bool hasValues = valuesFromC(
				engine.getManager(),
				userAgent.c_str(),
				requiredPropertyIndex,
				values);
			std::unique_ptr<ResultsHash> results(
				engine.process(userAgent.c_str()));
			const Value<std::vector<std::string>> actual =
				results->getValues(propertyName);
			ASSERT_EQ(hasValues, actual.hasValue()) << userAgent;
			if (hasValues == false) {
				continue;
			}
			for (const std::string &value : values) {
				expected.push_back(expectedScript(value));
				distinct.insert(value);
				if (value.empty()) {
					emptyValues++;
				}
			}
			ASSERT_EQ(expected, actual.getValue()) << userAgent;
			scripts += expected.size();
			if (expected.size() > mostValues) {
				mostValues = expected.size();
			}
		}
		const std::string summary = path + " (" + engine.getProduct() +
			"): " + std::to_string(scripts) + " scripts from " +
			std::to_string(userAgents.size()) + " User-Agents, " +
			std::to_string(distinct.size()) + " distinct values, " +
			std::to_string(emptyValues) + " of them from an empty value, " +
			"at most " + std::to_string(mostValues) + " for one User-Agent";
		printf("%s\n", summary.c_str());
		if (engine.getProduct() != "Lite") {
			EXPECT_GT(scripts, (size_t)0) << path;
		}
	}
}
