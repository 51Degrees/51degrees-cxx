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
#include "ExampleIpIntelligenceTests.hpp"
#include "../../examples/ipi/C/IpIntelligence/GettingStarted.c"

class ExampleTestGettingStarted : public ExampleIpIntelligenceTest {
public:
    void run(fiftyoneDegreesConfigIpi config) {
      // Capture stdout for the test.
      testing::internal::CaptureStdout();

      fiftyoneDegreesIpiGettingStarted(dataFilePath.c_str(), &config);

      // The Enterprise data file has every property the example prints.
      std::string output = testing::internal::GetCapturedStdout();
      if (dataFilePath.find("Enterprise") != std::string::npos) {
          EXPECT_EQ(output.find(FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE),
              std::string::npos) << output;
      }

      // Each value is shown with its weight, as
      // "- [<property>] (<type>) <x<weight>> <value>", and a weight is more
      // than 0 and at most 1.
      const std::regex weighted(
          R"(- \[[A-Za-z]+\] \((int|double|bool|byte|string)\) <x([^>]+)> )");
      int shown = 0;
      for (std::sregex_iterator match(output.begin(), output.end(), weighted),
          end; match != end; ++match) {
          const double weight = std::stod((*match)[2].str());
          EXPECT_GT(weight, 0.0) << (*match)[0].str();
          EXPECT_LE(weight, 1.0) << (*match)[0].str();
          shown++;
      }
      EXPECT_GT(shown, 0) << "No value was shown with its weight.\n" <<
          output;
    }
};

EXAMPLE_TESTS(ExampleTestGettingStarted)