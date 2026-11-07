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

#include "ExampleHashTests.hpp"
#include "../../../examples/dd/CPP/Hash/GettingStarted.cpp"

class ExampleCPPHashTestGettingStarted : public ExampleHashTest {
public:
	void run(fiftyoneDegreesConfigHash configHash) {
		// Capture stdout for the test.
		testing::internal::CaptureStdout();

		DeviceDetection::Hash::ConfigHash* config = 
			new DeviceDetection::Hash::ConfigHash(&configHash);
		GettingStarted* gettingStarted = new GettingStarted(
			dataFilePath, 
			config);
		gettingStarted->run();
		delete gettingStarted;

		// Only the free Lite data file lacks the properties the example
		// marks as needing the Enterprise data file.
		std::string output = testing::internal::GetCapturedStdout();
		bool lite = dataFilePath.find("Lite") != std::string::npos;
		EXPECT_EQ(lite, output.find(
			FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE) != std::string::npos) <<
			output;
		EXPECT_EQ(lite, output.find(
			"utm_term=lite-data-file") != std::string::npos) << output;
	}
};

EXAMPLE_HASH_TESTS(ExampleCPPHashTestGettingStarted)

// A fresh clone has the free Lite data file, which the example runs on.
TEST_F(ExampleCPPHashTestGettingStarted, LiteDataFile) {
	dataFilePath = getLiteDataFilePath();
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The Lite data file is not present.";
	}
	run(fiftyoneDegreesHashInMemoryConfig);
}