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

#include "ExampleIpIntelligenceTests.hpp"
#include "../../examples/ipi/C/IpIntelligence/FindProfiles.c"
#include <vector>

class ExampleTestFindProfiles : public ExampleIpIntelligenceTest {
public:
	void run(fiftyoneDegreesConfigIpi config) {
		// Capture stdout for the test.
		testing::internal::CaptureStdout();

		fiftyoneDegreesIpiFindProfiles(
			dataFilePath.c_str(),
			config);

		// Don't print the stdout
		std::string output = testing::internal::GetCapturedStdout();
	}
};

EXAMPLE_TESTS(ExampleTestFindProfiles)

/**
 * Country codes whose profiles are counted. Each one walks every profile in
 * the data set, so the list is kept short. It holds the country with the most
 * profiles, one with few and one between them.
 */
static const char * const countryCodes[] = { "US", "GB", "JP" };

/**
 * Counts the profiles that hold each country code, with the data set loaded
 * using the configuration provided.
 * @param dataFilePath path to the data file
 * @param config configuration to load the data set with
 * @return the count for each of the country codes, in the same order
 */
static std::vector<uint32_t> countProfilesForEachCountry(
	const char *dataFilePath,
	ConfigIpi config) {
	EXCEPTION_CREATE;
	ResourceManager manager;
	std::vector<uint32_t> counts;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = "RegisteredCountry";
	StatusCode status = IpiInitManagerFromFile(
		&manager,
		&config,
		&properties,
		dataFilePath,
		exception);
	EXPECT_EQ(SUCCESS, status) << "The data file could not be loaded.";
	if (status != SUCCESS) {
		return counts;
	}
	for (const char *code : countryCodes) {
		uint32_t found = 0;
		IpiIterateProfilesForPropertyAndValue(
			&manager,
			"RegisteredCountry",
			code,
			&found,
			count,
			exception);
		EXPECT_TRUE(EXCEPTION_OKAY) << code;
		counts.push_back(found);
	}
	ResourceManagerFree(&manager);
	return counts;
}

/**
 * Check that a data set which reads each profile from the file when it is
 * needed finds the same profiles for each value as one held in memory.
 */
TEST_F(ExampleTestFindProfiles, LowMemoryFindsTheSameProfilesAsInMemory) {
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The data file is not present.";
	}
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		GTEST_SKIP() << "Reading from a file is not built.";
	}
	const std::vector<uint32_t> inMemory = countProfilesForEachCountry(
		dataFilePath.c_str(),
		fiftyoneDegreesIpiInMemoryConfig);
	const std::vector<uint32_t> lowMemory = countProfilesForEachCountry(
		dataFilePath.c_str(),
		fiftyoneDegreesIpiLowMemoryConfig);
	EXPECT_EQ(inMemory, lowMemory);
}
