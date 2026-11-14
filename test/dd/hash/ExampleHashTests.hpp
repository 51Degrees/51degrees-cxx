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

#include <regex>
#include <set>
#include "../ExampleDeviceDetectionTests.hpp"

class ExampleHashTest : public ExampleDeviceDetectionTest {
public:
	ExampleHashTest() : ExampleDeviceDetectionTest(
		_HashFileNames,
        _HashFileNamesLength,
		_userAgentsFileName,
		_evidenceFileName) {}

protected:
	/**
	 * Path to the free Lite data file, which a fresh clone uses, or empty
	 * when it is not present.
	 */
	string getLiteDataFilePath() {
		return GetFilePath(_dataFolderName, "51Degrees-LiteV4.1.hash");
	}

	/**
	 * Expects every "Finished with hash code" line a reload example printed
	 * to give the same code. Each processing run reads the same User-Agents,
	 * and the data set is reloaded from the same file while they run, so
	 * processing during a reload must give the results it gives without one.
	 * @param output what the example printed
	 */
	void expectSameResultsWhileReloading(const string &output) {
		const std::regex finished("Finished with hash code '(-?[0-9]+)'");
		std::set<string> codes;
		int runs = 0;
		for (std::sregex_iterator match(
				output.begin(), output.end(), finished), end;
			match != end;
			++match) {
			codes.insert((*match)[1].str());
			runs++;
		}
		EXPECT_GT(runs, 0) << "No processing run finished.\n" << output;
		EXPECT_EQ((size_t)1, codes.size()) << "Processing while the data set "
			"was reloaded gave different results.\n" << output;
	}
};

#define EXAMPLE_HASH_TESTS(c) EXAMPLE_TESTS(c, Hash)