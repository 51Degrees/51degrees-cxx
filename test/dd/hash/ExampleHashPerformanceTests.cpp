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
#include "../../../examples/dd/C/Hash/Performance.c"

class ExampleHashTestPerformance : public ExampleHashTest {
public:
	std::string output;

	void run() {
		// Capture stdout for the test.
		testing::internal::CaptureStdout();

		fiftyoneDegreesHashPerformance(
			dataFilePath.c_str(),
			evidenceFilePath.c_str(),
			4,
			10000,
			stdout,
			NULL);

		// Don't print the stdout
		output = testing::internal::GetCapturedStdout();
	}
};

TEST_F(ExampleHashTestPerformance, All) {
    if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
        run();
    }
}

// The example asks for HardwareVendor, which the free Lite data file does not
// have. The benchmark must still finish, and say where to ask for the
// Enterprise data file.
TEST_F(ExampleHashTestPerformance, LiteDataFile) {
	dataFilePath = getLiteDataFilePath();
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The Lite data file is not present.";
	}
	if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
		GTEST_SKIP() << "The example does not run in the memory only build.";
	}
	run();
	EXPECT_NE(output.find("Properties retrieved 1\n"),
		std::string::npos)
		<< output;
	EXPECT_NE(output.find("utm_term=lite-data-file"), std::string::npos)
		<< output;
}
