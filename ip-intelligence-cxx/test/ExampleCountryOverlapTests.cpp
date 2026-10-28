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

#include <cstdio>
#include <string>
#include "ExampleIpIntelligenceTests.hpp"

// Sweep /16 chunks with a small cache rather than the /8 chunks and
// 600 MB per thread cache of a full run. CI provides the enterprise data
// file for every platform, so these tests always run there, and at /8
// chunks each configuration took over a minute and several GB. Every
// code path is still exercised because only the size of the work unit
// changes.
#define COUNTRY_OVERLAP_CHUNK_BITS 16
#define COUNTRY_OVERLAP_CACHE_BITS 16
#include "../examples/C/IpIntelligence/CountryOverlap.c"

/** CSV written by the test and removed afterwards. */
static const char testOutputPath[] = "country-overlap-test.csv";

/** First /16 chunk swept, 1.0.0.0. The chunks from 0.0.0.0 are reserved
and hold no locations, so starting there would exercise little. */
static const int testFirstChunk = 256;

/**
 * The country overlap example requires an enterprise data file with the
 * weighted country code properties, so the test skips rather than fails
 * when only the Lite data file is available. Two /16 chunks are swept
 * with two threads so the test completes quickly whilst still exercising
 * real data, the spot checks, and the CSV output.
 *
 * The tests are declared explicitly rather than via EXAMPLE_TESTS
 * because the low memory configuration is skipped. The sweep evaluates
 * millions of addresses and is only practical when the graph collection
 * is cached or loaded into memory. With the low memory configuration
 * every graph node read goes to the data file and a single test takes
 * upwards of an hour, so that configuration is not suitable for this
 * bulk analysis example.
 */
class ExampleTestCountryOverlap : public ExampleIpIntelligenceTest {
public:
    void run(fiftyoneDegreesConfigIpi config) {
        // Capture stdout for the test.
        testing::internal::CaptureStdout();

        const int result = fiftyoneDegreesIpiCountryOverlap(
            dataFilePath.c_str(),
            &config,
            testOutputPath,
            2,
            testFirstChunk,
            2,
            DEFAULT_MIN_SECONDARY_PERCENT);

        std::string output = testing::internal::GetCapturedStdout();

        if (result == COUNTRY_OVERLAP_PROPERTIES_MISSING) {
            GTEST_SKIP() <<
                "The data file does not include the weighted country "
                "code properties. An enterprise data file is required "
                "for the country overlap example.";
        }
        ASSERT_EQ(COUNTRY_OVERLAP_OK, result) <<
            "The country overlap example did not complete. Output: " <<
            output;

        // The spot checks compare the sweep against the normal lookup
        // process and every one must match.
        EXPECT_NE(
            output.find("spot checks match the normal lookup process"),
            std::string::npos) << output;
        EXPECT_EQ(output.find("MISMATCH"), std::string::npos) << output;
        EXPECT_EQ(output.find("LOOKUP FAILED"), std::string::npos) <<
            output;

        // The CSV file must exist and have the expected header row.
        expectCsvWithHeader(
            testOutputPath,
            "PrimaryCountryCode,PrimaryCountry");

        std::remove(testOutputPath);
    }

private:
    static void expectCsvWithHeader(
        const char* path,
        const char* expectedStart) {
        FILE* file = fopen(path, "r");
        ASSERT_NE(nullptr, file) <<
            "Expected the example to write '" << path << "'.";
        char line[256] = "";
        const char* read = fgets(line, sizeof(line), file);
        fclose(file);
        ASSERT_NE(nullptr, read) <<
            "Expected '" << path << "' to contain a header row.";
        EXPECT_EQ(0, strncmp(line, expectedStart, strlen(expectedStart)))
            << "Unexpected header in '" << path << "': " << line;
    }
};

TEST_F(ExampleTestCountryOverlap, Default) {
    if (shouldSkipTempFileTestOnCI(fiftyoneDegreesIpiDefaultConfig)) {
        GTEST_SKIP() << "Skipping temp file test on CI";
    }
    if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
        run(fiftyoneDegreesIpiDefaultConfig);
    }
}
TEST_F(ExampleTestCountryOverlap, BalancedTemp) {
    if (shouldSkipTempFileTestOnCI(fiftyoneDegreesIpiBalancedTempConfig)) {
        GTEST_SKIP() << "Skipping temp file test on CI";
    }
    if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
        run(fiftyoneDegreesIpiBalancedTempConfig);
    }
}
TEST_F(ExampleTestCountryOverlap, Balanced) {
    if (shouldSkipTempFileTestOnCI(fiftyoneDegreesIpiBalancedConfig)) {
        GTEST_SKIP() << "Skipping temp file test on CI";
    }
    if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
        run(fiftyoneDegreesIpiBalancedConfig);
    }
}
TEST_F(ExampleTestCountryOverlap, LowMemory) {
    GTEST_SKIP() <<
        "The country overlap sweep evaluates millions of addresses "
        "and needs the graph collection cached or loaded into memory. "
        "The low memory configuration reads every graph node from the "
        "data file which takes upwards of an hour, so it is not "
        "suitable for this bulk analysis example.";
}
TEST_F(ExampleTestCountryOverlap, HighPerformance) {
    if (shouldSkipTempFileTestOnCI(
        fiftyoneDegreesIpiHighPerformanceConfig)) {
        GTEST_SKIP() << "Skipping temp file test on CI";
    }
    run(fiftyoneDegreesIpiHighPerformanceConfig);
}
/**
 * An output path that cannot be written must fail the run before the
 * sweep starts. Previously the CSV was only opened after the sweep, so a
 * full run took an hour and then reported success without its output.
 */
TEST_F(ExampleTestCountryOverlap, UnwritableOutputFails) {
    if (fiftyoneDegreesCollectionGetIsMemoryOnly() == true) {
        GTEST_SKIP() << "Requires a file based configuration.";
    }
    testing::internal::CaptureStdout();
    const int result = fiftyoneDegreesIpiCountryOverlap(
        dataFilePath.c_str(),
        &fiftyoneDegreesIpiBalancedConfig,
        "missing-directory-for-country-overlap/output.csv",
        2,
        testFirstChunk,
        2,
        DEFAULT_MIN_SECONDARY_PERCENT);
    std::string output = testing::internal::GetCapturedStdout();
    if (result == COUNTRY_OVERLAP_PROPERTIES_MISSING) {
        GTEST_SKIP() <<
            "The data file does not include the weighted country "
            "code properties. An enterprise data file is required "
            "for the country overlap example.";
    }
    EXPECT_EQ(COUNTRY_OVERLAP_FAILED, result) << output;
    EXPECT_NE(output.find("Could not open"), std::string::npos) << output;
    EXPECT_EQ(output.find("Sweeping"), std::string::npos) <<
        "The sweep must not start when the output cannot be written. " <<
        output;
}
TEST_F(ExampleTestCountryOverlap, InMemory) {
    if (shouldSkipTempFileTestOnCI(fiftyoneDegreesIpiInMemoryConfig)) {
        GTEST_SKIP() << "Skipping temp file test on CI";
    }
    run(fiftyoneDegreesIpiInMemoryConfig);
}

/**
 * Every index valueIndex returns must name a printable value, because the
 * console summary and every CSV row print the name for the index they were
 * recorded against. The names used to be allocated per entry, so a failed
 * allocation stored a null that the reporting then dereferenced. They are
 * now copied into the table, which removes the failure case; this test
 * holds the invariant so the table cannot quietly go back to pointers.
 *
 * Needs no data file, so unlike the sweep tests it runs everywhere.
 */
TEST(CountryOverlapValueTable, EveryIndexNamesAPrintableValue) {
    ValueTable table;
    memset(&table, 0, sizeof(table));

    // Fill past the table's capacity so the saturating path is covered.
    for (int value = 0; value < VALUE_SLOTS + 4; value++) {
        char name[32];
        snprintf(name, sizeof(name), "Value%d", value);
        const int index = valueIndex(&table, name);
        ASSERT_GE(index, 0);
        ASSERT_LT(index, VALUE_SLOTS);
        EXPECT_NE('\0', table.names[index][0]) <<
            "valueIndex returned slot " << index <<
            " for '" << name << "' but that slot holds no name.";
    }
    EXPECT_EQ(VALUE_SLOTS, table.count);
}

/** A name already in the table must return its existing index rather than
consume another slot, because the merge relies on equal names mapping to
one index across threads. */
TEST(CountryOverlapValueTable, RepeatedNameReusesItsIndex) {
    ValueTable table;
    memset(&table, 0, sizeof(table));

    const int first = valueIndex(&table, "Broadband");
    const int second = valueIndex(&table, "Cellular");
    EXPECT_EQ(first, valueIndex(&table, "Broadband"));
    EXPECT_EQ(second, valueIndex(&table, "Cellular"));
    EXPECT_EQ(2, table.count);
}

/** A name longer than the table's storage must be truncated into it and
stay NUL terminated rather than overrun the slot. */
TEST(CountryOverlapValueTable, OverlongNameIsTruncatedNotOverrun) {
    ValueTable table;
    memset(&table, 0, sizeof(table));

    std::string overlong(VALUE_NAME_MAX * 2, 'x');
    const int index = valueIndex(&table, overlong.c_str());
    ASSERT_GE(index, 0);
    ASSERT_LT(index, VALUE_SLOTS);
    EXPECT_EQ(
        VALUE_NAME_MAX - 1,
        (int)strlen(table.names[index])) <<
        "The name should fill the slot and stop short of its last byte.";
}
