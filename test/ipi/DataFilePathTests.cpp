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
 * @file DataFilePathTests.cpp
 * @brief Tests that a data file opens through a path written with each
 * separator, and that a file which cannot be opened is named in the message.
 */

#include "pch.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

/** The small data file in the data repository, which is always present. */
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

static std::string withSeparator(std::string path, char from, char to) {
	std::replace(path.begin(), path.end(), from, to);
	return path;
}

/**
 * Checks the exception names the file, rather than saying 'null', when the
 * call provided cannot open it.
 */
template <typename F>
static void expectFileNamed(const char *path, F call) {
	try {
		call();
		FAIL() << "No exception was thrown for " << path;
	}
	catch (StatusCodeException &e) {
		EXPECT_NE(nullptr, strstr(e.what(), path)) << e.what();
		EXPECT_EQ(nullptr, strstr(e.what(), "'null'")) << e.what();
	}
}

class DataFilePathTests : public Base {
public:
	DataFilePathTests() {
		dataFilePath = GetFilePath(_dataFolderName, asnFileName);
	}

protected:
	std::string dataFilePath;

	/**
	 * Checks the data file opens through the path using each kind of
	 * configuration, those which read from the file and the one which does
	 * not.
	 */
	static void expectOpens(const std::string &path) {
		ConfigIpi configs[3];
		configs[0].setBalanced();
		configs[1].setLowMemory();
		configs[2].setMaxPerformance();
		for (ConfigIpi &config : configs) {
			RequiredPropertiesConfig properties;
			EXPECT_NO_THROW(EngineIpi(path.c_str(), &config, &properties)) <<
				path;
		}
	}
};

/**
 * Check the data file opens through a path written with each separator the
 * system has. A backslash is an ordinary character in a file name outside
 * Windows.
 */
TEST_F(DataFilePathTests, OpensWithEachSeparator) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The data file " << asnFileName << " was not found.";
	}
	expectOpens(withSeparator(dataFilePath, '\\', '/'));
#ifdef _WIN32
	expectOpens(withSeparator(dataFilePath, '/', '\\'));
#endif
}

#ifdef _WIN32
/**
 * Check the data file opens through a network path written with each separator.
 * The path goes through this computer's administrative share for the drive, so
 * the test is skipped where that share is not available.
 */
TEST_F(DataFilePathTests, OpensNetworkPathWithEachSeparator) {
	if (dataFilePath.size() < 3 || dataFilePath[1] != ':') {
		GTEST_SKIP() << "The data file is not on a drive: " << dataFilePath;
	}
	const std::string back = withSeparator(
		std::string("\\\\localhost\\") + dataFilePath[0] + "$" +
		dataFilePath.substr(2),
		'/',
		'\\');
	const std::string forward = withSeparator(back, '\\', '/');
	if (fiftyoneDegreesFileGetSize(forward.c_str()) <= 0) {
		GTEST_SKIP() << "The administrative share is not available: " << forward;
	}
	expectOpens(forward);
	expectOpens(back);
}
#endif

/**
 * Check a data file that cannot be opened is named in the message, through a
 * path written with each separator.
 */
TEST_F(DataFilePathTests, MissingFileIsNamed) {
	const char *paths[] = {
		"missing-folder/missing.ipi",
		"missing-folder\\missing.ipi",
		"//localhost/missing-share/missing.ipi",
		"\\\\localhost\\missing-share\\missing.ipi"
	};
	for (const char *path : paths) {
		ConfigIpi config;
		config.setLowMemory();
		RequiredPropertiesConfig properties;
		expectFileNamed(path, [&]() {
			EngineIpi engine(path, &config, &properties);
		});
	}
}

/**
 * Check a reload that cannot open its file names that file, whether the file
 * was given or is the one the engine was created from.
 */
TEST_F(DataFilePathTests, FailedReloadNamesTheFile) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The data file " << asnFileName << " was not found.";
	}
	const char *copy = "reload-named.ipi";
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesFileCopy(dataFilePath.c_str(), copy));
	{
		ConfigIpi config;
		config.setMaxPerformance();
		RequiredPropertiesConfig properties;
		EngineIpi engine(copy, &config, &properties);
		const char *missing = "missing-folder/missing.ipi";
		expectFileNamed(missing, [&]() { engine.refreshData(missing); });
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileDelete(copy));
		expectFileNamed(copy, [&]() { engine.refreshData(); });
	}
}
