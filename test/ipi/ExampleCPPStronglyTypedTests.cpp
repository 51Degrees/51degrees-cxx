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
#include "../../examples/ipi/Base/ExampleBase.h"
#include "../../examples/ipi/CPP/IpIntelligence/StronglyTyped.cpp"

class ExampleCPPTestStronglyTyped : public ExampleIpIntelligenceTest {
public:
	void run() {
		// The example opens the data file with the default configuration,
		// which reads from the file.
		if (fiftyoneDegreesCollectionGetIsMemoryOnly()) {
			return;
		}

		// Capture stdout for the test.
		testing::internal::CaptureStdout();
		{
			StronglyTyped example(dataFilePath);
			example.run();
		}

		// Only a free data file lacks the properties the example marks as
		// needing the Enterprise data file, and only there does the example
		// say where to ask for a copy.
		std::string output = testing::internal::GetCapturedStdout();
		bool enterprise =
			dataFilePath.find("Enterprise") != std::string::npos;
		EXPECT_NE(output.find("Ipv4 Address: "), std::string::npos) << output;
		EXPECT_EQ(enterprise, output.find(
			FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE) == std::string::npos) <<
			output;
		EXPECT_EQ(enterprise, output.find(
			"utm_term=free-data-file") == std::string::npos) << output;
	}
};

// The data file the other example tests use.
TEST_F(ExampleCPPTestStronglyTyped, DataFile) {
	run();
}

// A fresh clone has the free ASN data file, which has none of the properties
// the example prints.
TEST_F(ExampleCPPTestStronglyTyped, AsnDataFile) {
	dataFilePath = getAsnDataFilePath();
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The free ASN data file is not present.";
	}
	run();
}
