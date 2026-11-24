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
#include <fstream>
#include "../../../examples/dd/C/Hash/ProcHash.c"

/**
 * Writes the User-Agent as a line of the file.
 * @param userAgent to write
 * @param state the std::ofstream to write to
 */
static void writeUserAgent(const char *userAgent, void *state) {
	*(std::ofstream*)state << userAgent << "\n";
}

class ExampleTestHashProc : public ExampleHashTest {
public:
	void run(fiftyoneDegreesConfigHash config) {
		string line;
		int lines = 0;

		// The example reads one User-Agent from each line of stdin, so
		// write the User-Agents of the evidence file to a file of lines.
		const string userAgentsPath = testing::TempDir() +
			"51degrees-prochash-" +
			testing::UnitTest::GetInstance()->current_test_info()->name() +
			".txt";
		{
			char userAgent[1000] = "";
			std::ofstream userAgents(userAgentsPath);
			fiftyoneDegreesExampleUserAgentsIterate(
				evidenceFilePath.c_str(),
				userAgent,
				sizeof(userAgent),
				&userAgents,
				writeUserAgent);
		}

		// Associate the file of User-Agents with stdin.
#ifdef _MSC_VER
		FILE *input = nullptr;
		ASSERT_EQ(0, freopen_s(&input, userAgentsPath.c_str(), "r", stdin))
			<< userAgentsPath;
#else
		ASSERT_NE(nullptr, freopen(userAgentsPath.c_str(), "r", stdin))
			<< userAgentsPath;
#endif

		// Capture stdout for the test.
		testing::internal::CaptureStdout();

		// Start to process User-Agents.
		int count = fiftyoneDegreesProcHashRun(
			dataFilePath.c_str(),
			"IsMobile",
			&config);

		// Get the output from the processing.
		stringstream output = stringstream(
			testing::internal::GetCapturedStdout());

		// Loop through the output lines.
		while (getline(output, line)) {
			lines++;
		}

		// Check lines in and out are the same, and that there were some.
		EXPECT_EQ(count, lines) << "Same number of User-Agents in and out required";
		EXPECT_GT(count, 0) << "No User-Agents were read from " << evidenceFilePath;
	}
};

EXAMPLE_HASH_TESTS(ExampleTestHashProc)
