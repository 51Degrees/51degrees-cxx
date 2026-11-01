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

#include "../Constants.hpp"
#include "../EngineDeviceDetectionTests.hpp"
#include "../../../src/dd/hash/EngineHash.hpp"
#include "HashTestMemory.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using std::ifstream;
using std::ofstream;
using std::ios;

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

class EngineHashInitTests : public Base {
public:
    const char* badVersionFileName = "badVersion.hash";
    const char* badDataFileName = "badData.hash";
    const char* smallDataFileName = "smallData.hash";
    const int32_t badVersion[4] = { 1, 2, 3, 4 };
    const int32_t goodVersion[4] = { 4, 1, 0, 0 };
    virtual void SetUp() {
        Base::SetUp();
        writeTestFiles();
    }
    virtual void TearDown() {
        removeTestFiles();
        Base::TearDown();
    }

    /**
     * Finds a real data file, the smallest first.
     * @return the path to the data file, or an empty string if there is none
     */
    std::string findDataFile() {
        std::string path;
        for (int i = _HashFileNamesLength - 1; i >= 0 && path.empty(); i--) {
            path = GetFilePath(_dataFolderName, _HashFileNames[i]);
        }
        return path;
    }

    /**
     * Check that when initializing from a file which has the wrong version,
     * the correct error is thrown, and memory is cleaned up.
     */
    void wrongDataVersionFile(ConfigHash* config) {
        RequiredPropertiesConfig properties;
        try {
            EngineHash* testEngine = new EngineHash(badVersionFileName, config, &properties);
            delete testEngine;
            FAIL() << L"No exception was thrown";
        }
        catch (exception & e) {
            const char* expected = fiftyoneDegreesStatusGetMessage(
                FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
                badVersionFileName);
            ASSERT_STREQ(
                e.what(),
                expected);
            fiftyoneDegreesFree((void*)expected);

        }
    }
    
    /**
     * Check that when initializing from a file which has the correct version
     * but corrupted data, the correct error is thrown, and memory is cleaned up.
     */
    void badDataFile(ConfigHash* config) {
        RequiredPropertiesConfig properties;
        try {
            EngineHash* testEngine = new EngineHash(badDataFileName, config, &properties);
            delete testEngine;
            FAIL() << L"No exception was thrown";
        }
        catch (exception & e) {
            const char* expected = fiftyoneDegreesStatusGetMessage(
                FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
                badDataFileName);
            ASSERT_STREQ(
                e.what(),
                expected);
            fiftyoneDegreesFree((void*)expected);
        }
    }

    /**
     * Check that when initializing from a file which is too small and does not
     * contain enough data to fill the header, the correct error is thrown,
     * and memory is cleaned up.
     */
    void smallDataFile(ConfigHash *config) {
        RequiredPropertiesConfig properties;
        try {
            EngineHash* testEngine = new EngineHash(smallDataFileName, config, &properties);
            delete testEngine;
            FAIL() << L"No exception was thrown";
        }
        catch (exception & e) {
            const char* expected = fiftyoneDegreesStatusGetMessage(
                FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
                smallDataFileName);
            ASSERT_STREQ(
                e.what(),
                expected);
            fiftyoneDegreesFree((void*)expected);

        }
    }

    /**
     * @return the number of bytes in the named file, or zero if it cannot be
     * opened.
     */
    static size_t fileLength(const char *fileName) {
        ifstream file(fileName, ios::in | ios::binary | ios::ate);
        if (file.is_open() == false) {
            cout << "Unable to open file";
            return 0;
        }
        const std::streampos length = file.tellg();
        file.close();
        return (size_t)length;
    }

private:
    void writeTestFiles() {
        void* garbledHeader =
            malloc(sizeof(fiftyoneDegreesDataSetHashHeader));
        for (size_t i = 0; i < sizeof(fiftyoneDegreesDataSetHashHeader); i++) {
            ((byte*)garbledHeader)[i] = 12;
        }
        
        ofstream badVersionFile(badVersionFileName, ios::out | ios::binary);
        if (badVersionFile.is_open()) {
            badVersionFile.write((char*)badVersion, sizeof(int32_t) * 4);
            badVersionFile.write((char*)garbledHeader, sizeof(fiftyoneDegreesDataSetHashHeader));
            badVersionFile.close();
        }
        else {
            cout << "Unable to open file";
        }

        ofstream badDataFile(badDataFileName, ios::out | ios::binary);
        if (badDataFile.is_open()) {
            badDataFile.write((char*)goodVersion, sizeof(int32_t) * 4);
            badDataFile.write((char*)garbledHeader, sizeof(fiftyoneDegreesDataSetHashHeader));
            badDataFile.close();
        }
        else {
            cout << "Unable to open file";
        }

        ofstream smallDataFile(smallDataFileName, ios::out | ios::binary);
        if (smallDataFile.is_open()) {
            smallDataFile.write((char*)garbledHeader, sizeof(byte));
            smallDataFile.close();
        }
        else {
            cout << "Unable to open file";
        }
        free(garbledHeader);
    }
    void removeTestFiles() {
        if (remove(badVersionFileName) != 0) {
            cout << "Error deleting file";
        }
        if (remove(badDataFileName) != 0) {
            cout << "Error deleting file";
        }
        if (remove(smallDataFileName) != 0) {
            cout << "Error deleting file";
        }
    }
};

#define TEST_WRONG_VERSION_FILE(c) \
TEST_F(EngineHashInitTests,WrongDataVersion_File_##c) { \
    ConfigHash config; \
    config.set##c(); \
    wrongDataVersionFile(&config); \
}

TEST_WRONG_VERSION_FILE(HighPerformance);
TEST_WRONG_VERSION_FILE(LowMemory);
TEST_WRONG_VERSION_FILE(Balanced);
TEST_WRONG_VERSION_FILE(BalancedTemp);

#define TEST_BAD_DATA_FILE(c) \
TEST_F(EngineHashInitTests,BadData_File_##c) { \
    ConfigHash config; \
    config.set##c(); \
    badDataFile(&config); \
}

TEST_BAD_DATA_FILE(HighPerformance);
TEST_BAD_DATA_FILE(LowMemory);
TEST_BAD_DATA_FILE(Balanced);
TEST_BAD_DATA_FILE(BalancedTemp);

#define TEST_SMALL_DATA_FILE(c) \
TEST_F(EngineHashInitTests,SmallData_File_##c) { \
    ConfigHash config; \
    config.set##c(); \
    smallDataFile(&config); \
}

TEST_SMALL_DATA_FILE(HighPerformance);
TEST_SMALL_DATA_FILE(LowMemory);
TEST_SMALL_DATA_FILE(Balanced);
TEST_SMALL_DATA_FILE(BalancedTemp);

/**
 * Check that when initializing from memory which has the wrong version,
 * the correct error is thrown, and memory is cleaned up.
 */
TEST_F(EngineHashInitTests, WrongDataVersion_Memory) {
    ConfigHash config;
    RequiredPropertiesConfig properties;
    void* mem = fiftyoneDegreesMalloc(sizeof(fiftyoneDegreesDataSetHashHeader));
    ifstream file(badVersionFileName, ios::out | ios::binary);
    if (file.is_open()) {
        file.read((char*)mem, sizeof(fiftyoneDegreesDataSetHashHeader));
        file.close();
    }
    else {
        cout << "Unable to open file";
    }

    try {
        EngineHash* testEngine = new EngineHash(
            mem,
            (long)sizeof(fiftyoneDegreesDataSetHashHeader),
            &config,
            &properties);
        delete testEngine;
        FAIL() << L"No exception was thrown";
    }
    catch (exception & e) {
        const char* expected = fiftyoneDegreesStatusGetMessage(
            FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
            NULL);
        ASSERT_STREQ(
            e.what(),
            expected);
        fiftyoneDegreesFree((void*)expected);
    }
    fiftyoneDegreesFree(mem);
}

/**
 * Check that when initializing from memory which has the correct version
 * but corrupted data, the correct error is thrown, and memory is cleaned up.
 */
TEST_F(EngineHashInitTests, BadData_Memory) {
    ConfigHash config;
    RequiredPropertiesConfig properties;
    void* mem = fiftyoneDegreesMalloc(sizeof(fiftyoneDegreesDataSetHashHeader));
    ifstream file(badDataFileName, ios::out | ios::binary);
    if (file.is_open()) {
        file.read((char*)mem, sizeof(fiftyoneDegreesDataSetHashHeader));
        file.close();
    }
    else {
        cout << "Unable to open file";
    }

    try {
        EngineHash* testEngine = new EngineHash(
            mem,
            (long)sizeof(fiftyoneDegreesDataSetHashHeader),
            &config,
            &properties);
        delete testEngine;
        FAIL() << L"No exception was thrown";
    }
    catch (exception & e) {
        const char* expected = fiftyoneDegreesStatusGetMessage(
            FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
            NULL);
        ASSERT_STREQ(
            e.what(),
            expected);
        fiftyoneDegreesFree((void*)expected);
    }
    fiftyoneDegreesFree(mem);
}

/**
 * Check that when initializing from memory which is too small and does not
 * contain enough data to fill the header, the correct error is thrown,
 * and memory is cleaned up.
 *
 * The buffer is the size of the data rather than the size of the header, so
 * that reading a header out of it is an out of bounds read a sanitizer or an
 * unmapped page can catch.
 */
TEST_F(EngineHashInitTests, SmallData_Memory) {
    ConfigHash config;
    RequiredPropertiesConfig properties;
    const size_t dataLength = fileLength(smallDataFileName);
    ASSERT_LT(dataLength, sizeof(fiftyoneDegreesDataSetHashHeader))
        << "The small data file must be shorter than the header for this "
        << "test to exercise a short buffer.";
    void* mem = fiftyoneDegreesMalloc(dataLength);
    ifstream file(smallDataFileName, ios::in | ios::binary);
    if (file.is_open()) {
        file.read((char*)mem, dataLength);
        file.close();
    }
    else {
        cout << "Unable to open file";
    }

    try {
        EngineHash* testEngine = new EngineHash(
            mem,
            (long)dataLength,
            &config,
            &properties);
        delete testEngine;
        FAIL() << L"No exception was thrown";
    }
    catch (exception & e) {
        const char* expected = fiftyoneDegreesStatusGetMessage(
            FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
            NULL);
        ASSERT_STREQ(
            e.what(),
            expected);
        fiftyoneDegreesFree((void*)expected);
    }
    fiftyoneDegreesFree(mem);
}

static void expectThrows(
    TestMemory &memory,
    fiftyoneDegreesStatusCode expected) {
    ConfigHash config;
    RequiredPropertiesConfig properties;
    try {
        EngineHash engine(
            (void*)memory.data(),
            (long)memory.size(),
            &config,
            &properties);
        ADD_FAILURE() << "No exception was thrown";
    }
    catch (StatusCodeException & e) {
        EXPECT_EQ(expected, e.getCode());
    }
}

/**
 * Expects the constructor from memory to throw the status provided and to leave
 * nothing allocated, including the copy it takes of the memory. It runs with
 * tracking, which every build can check, then without, as the address sanitizer
 * cannot see a leak while tracking holds a pointer to the memory.
 */
static void expectFreesCopy(
    TestMemory &memory,
    fiftyoneDegreesStatusCode expected) {
    fiftyoneDegreesSetUpMemoryTracking();
    expectThrows(memory, expected);
    EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
    expectThrows(memory, expected);
}

/**
 * Check the copy is freed when the init method returns a failed status.
 */
TEST_F(EngineHashInitTests, Garbage_Memory_FreesCopy) {
    TestMemory memory = garbage(64 * 1024);
    expectFreesCopy(memory, FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION);
}

/**
 * Check the copy is freed when only the exception shows the failure, as the
 * init method returns success with no data set.
 */
TEST_F(EngineHashInitTests, ComponentMissing_Memory_FreesCopy) {
    TestMemory memory = emptyDataSet();
    setCollection(
        memory,
        offsetof(TestHeader, components),
        sizeof(TestHeader),
        0,
        1);
    expectFreesCopy(
        memory,
        FIFTYONE_DEGREES_STATUS_COLLECTION_OFFSET_OUT_OF_RANGE);
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
    catch (StatusCodeException & e) {
        EXPECT_NE(nullptr, strstr(e.what(), path)) << e.what();
        EXPECT_EQ(nullptr, strstr(e.what(), "'null'")) << e.what();
    }
}

/**
 * Check a data file that cannot be opened is named in the message, through a
 * path written with each separator.
 */
TEST_F(EngineHashInitTests, MissingFile_Named) {
    const char *paths[] = {
        "missing-folder/missing.hash",
        "missing-folder\\missing.hash",
        "//localhost/missing-share/missing.hash",
        "\\\\localhost\\missing-share\\missing.hash"
    };
    for (const char *path : paths) {
        ConfigHash config;
        config.setLowMemory();
        RequiredPropertiesConfig properties;
        expectFileNamed(path, [&]() {
            EngineHash engine(path, &config, &properties);
        });
    }
}

/**
 * Check a reload that cannot open its file names that file, whether the file
 * was given or is the one the engine was created from.
 */
TEST_F(EngineHashInitTests, FailedReload_Named) {
    const std::string dataFile = findDataFile();
    ASSERT_FALSE(dataFile.empty()) << "No Hash data file was found.";
    const char *copy = "reload-named.hash";
    ASSERT_EQ(
        FIFTYONE_DEGREES_STATUS_SUCCESS,
        fiftyoneDegreesFileCopy(dataFile.c_str(), copy));
    {
        ConfigHash config;
        config.setMaxPerformance();
        RequiredPropertiesConfig properties("IsMobile");
        EngineHash engine(copy, &config, &properties);
        const char *missing = "missing-folder/missing.hash";
        expectFileNamed(missing, [&]() { engine.refreshData(missing); });
        ASSERT_EQ(
            FIFTYONE_DEGREES_STATUS_SUCCESS,
            fiftyoneDegreesFileDelete(copy));
        expectFileNamed(copy, [&]() { engine.refreshData(); });
    }
}

/**
 * Check a data file opens through a path written with each separator the
 * system has, using the configurations which read from the file.
 */
TEST_F(EngineHashInitTests, OpensWithEachSeparator) {
    std::string path = findDataFile();
    ASSERT_FALSE(path.empty()) << "No Hash data file was found.";
    std::replace(path.begin(), path.end(), '\\', '/');
    std::vector<std::string> paths = { path };
#ifdef _WIN32
    std::replace(path.begin(), path.end(), '/', '\\');
    paths.push_back(path);
#endif
    for (const std::string &current : paths) {
        ConfigHash balanced;
        balanced.setBalanced();
        ConfigHash lowMemory;
        lowMemory.setLowMemory();
        RequiredPropertiesConfig properties("IsMobile");
        EXPECT_NO_THROW(EngineHash(current.c_str(), &balanced, &properties)) <<
            current;
        EXPECT_NO_THROW(EngineHash(current.c_str(), &lowMemory, &properties)) <<
            current;
    }
}

/**
 * Gets the number of properties an engine makes available.
 */
static int availableProperties(EngineHash &engine) {
    std::unique_ptr<ResultsHash> results(engine.process("Mozilla/5.0"));
    return results->getAvailableProperties();
}

/**
 * Check null required properties mean every property for the constructors
 * from a file and from memory, as they do for the C init functions.
 */
TEST_F(EngineHashInitTests, NullProperties_LoadsEveryProperty) {
    const std::string path = findDataFile();
    ASSERT_FALSE(path.empty()) << "No Hash data file was found.";
    ConfigHash config;
    RequiredPropertiesConfig every;
    EngineHash expected(path.c_str(), &config, &every);
    const int count = availableProperties(expected);
    ASSERT_GT(count, 0);

    EngineHash fromFile(path.c_str(), &config, nullptr);
    EXPECT_EQ(count, availableProperties(fromFile));

    std::ifstream file(path, ios::binary | ios::ate);
    std::vector<char> data((size_t)file.tellg());
    file.seekg(0, ios::beg);
    ASSERT_TRUE((bool)file.read(data.data(), (std::streamsize)data.size()));
    EngineHash fromMemory(
        (void*)data.data(),
        (long)data.size(),
        &config,
        nullptr);
    EXPECT_EQ(count, availableProperties(fromMemory));
}
