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
 * @file HashProductTests.cpp
 * @brief Tests that the product name read from the header of a Hash data
 * file, by the C function and by the engine, is "V4" followed by the tier an
 * engine that loads the file gives, and that a file that is missing, cut
 * short or of another kind gives a status and no name.
 *
 * The tests that need other names write a file holding the header of the
 * data file found, changed so that the name is a tier the test writes after
 * it.
 */

#include "../../common/pch.h"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/hash.h"
#include "../../../src/dd/hash/EngineHash.hpp"

using FiftyoneDegrees::DeviceDetection::Hash::EngineHash;

/** The start of every Hash product name, before the tier. */
static const std::string productPrefix = "V4";

/** Bytes for a product name that every test name fits in. */
static const size_t productNameBytes = 256;

/** The IP Intelligence data files, which are of another kind. */
static const char ipiDataFolderName[] = "data/ip-intelligence";
static const char* const ipiFileNames[] = {
	"51Degrees-IPIV4AsnIpiV41.ipi",
	"51Degrees-LiteV41.ipi",
	"51Degrees-EnterpriseIpiV41.ipi" };

/** The version of the IP Intelligence data files. */
static const int32_t ipiVersionMajor = 4;
static const int32_t ipiVersionMinor = 6;

/** A file the tests below write and remove. */
static const std::string productFileNameValue =
	testProcessName("HashProductTests-FromFile") + ".hash";
static const char *productFileName = productFileNameValue.c_str();

/**
 * The product name the Distributor knows a tier by, as the data updates
 * specification gives it: the prefix followed by the tier, or the tier alone
 * when it already starts with the prefix in any case.
 */
static std::string expectedProduct(const std::string &tier) {
	if (tier.size() >= productPrefix.size()) {
		bool same = true;
		for (size_t i = 0; i < productPrefix.size(); i++) {
			if (toupper((unsigned char)tier[i]) !=
				toupper((unsigned char)productPrefix[i])) {
				same = false;
			}
		}
		if (same) {
			return tier;
		}
	}
	return productPrefix + tier;
}

/**
 * The status, product name and bytes needed from the C function.
 */
struct ProductResult {
	fiftyoneDegreesStatusCode status;
	std::string name;
	size_t needed;
};

/**
 * Gets the product name from the C function with a buffer of the length
 * given, which starts full of a character that is not null so a test sees
 * what is written.
 */
static ProductResult getProduct(const char *path, size_t length) {
	std::vector<char> buffer(length + 1, 'x');
	ProductResult result;
	result.needed = 12345;
	result.status = fiftyoneDegreesHashGetProductFromFile(
		path,
		length == 0 ? nullptr : buffer.data(),
		length,
		&result.needed);
	buffer[length] = '\0';
	result.name = std::string(buffer.data());
	return result;
}

/**
 * Gets the status the engine throws for a file that gives no product name,
 * or fails the test when it gives a name instead.
 */
static fiftyoneDegreesStatusCode getEngineStatus(const char *path) {
	try {
		const std::string name = EngineHash::getProductFromFile(path);
		ADD_FAILURE() << "'" << name << "' was given for '" << path << "'.";
	}
	catch (const FiftyoneDegrees::Common::StatusCodeException &e) {
		return e.getCode();
	}
	return FIFTYONE_DEGREES_STATUS_SUCCESS;
}

class HashProductTests : public Base {
protected:
	std::vector<unsigned char> header;

	void SetUp() override {
		Base::SetUp();
		std::string path;
		for (int i = _HashFileNamesLength - 1; i >= 0 && path.empty(); i--) {
			path = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		ASSERT_FALSE(path.empty()) << "No data file was found.";
		ASSERT_NO_FATAL_FAILURE(readStart(
			path,
			sizeof(fiftyoneDegreesDataSetHashHeader),
			&header));
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::vector<unsigned char>().swap(header);
		remove(productFileName);
		Base::TearDown();
	}

	/**
	 * Reads the first length bytes of a file.
	 */
	static void readStart(
		const std::string &path,
		size_t length,
		std::vector<unsigned char> *bytes) {
		FILE *file = nullptr;
		fiftyoneDegreesFileOpen(path.c_str(), &file);
		ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
		bytes->resize(length);
		const size_t read = fread(bytes->data(), 1, length, file);
		fclose(file);
		ASSERT_EQ(length, read) << "Could not read '" << path << "'.";
	}

	void setInt32(size_t position, int32_t value) {
		memcpy(header.data() + position, &value, sizeof(value));
	}

	/**
	 * Writes the header, changed so the name is at the start of a strings
	 * collection that follows it, then the size given and the first length
	 * bytes of the tier and its terminating null.
	 */
	void writeFile(
		const std::string &tier,
		uint16_t size,
		size_t length) {
		std::vector<unsigned char> bytes(header);
		const fiftyoneDegreesFileOffsetUnsigned start =
			(fiftyoneDegreesFileOffsetUnsigned)bytes.size();
		const int32_t nameOffset = 0;
		memcpy(
			bytes.data() +
				offsetof(fiftyoneDegreesDataSetHashHeader, strings) +
				offsetof(fiftyoneDegreesCollectionHeader, startPosition),
			&start,
			sizeof(start));
		memcpy(
			bytes.data() +
				offsetof(fiftyoneDegreesDataSetHashHeader, nameOffset),
			&nameOffset,
			sizeof(nameOffset));
		const unsigned char *sizeBytes = (const unsigned char*)&size;
		bytes.insert(bytes.end(), sizeBytes, sizeBytes + sizeof(size));
		const char *text = tier.c_str();
		bytes.insert(bytes.end(), text, text + length);
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(
				productFileName,
				bytes.data(),
				bytes.size())) <<
			"Could not write '" << productFileName << "'.";
	}

	/**
	 * Changes the name offset in the file the tests write, which holds the
	 * tier "TAC".
	 */
	void setWrittenNameOffset(int32_t nameOffset) {
		std::vector<unsigned char> bytes;
		ASSERT_NO_FATAL_FAILURE(readStart(
			productFileName,
			header.size() + sizeof(uint16_t) + sizeof("TAC"),
			&bytes));
		memcpy(
			bytes.data() +
				offsetof(fiftyoneDegreesDataSetHashHeader, nameOffset),
			&nameOffset,
			sizeof(nameOffset));
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(
				productFileName,
				bytes.data(),
				bytes.size()));
	}

	/**
	 * Writes the tier whole, with its size counting the terminating null.
	 */
	void writeTier(const std::string &tier) {
		writeFile(tier, (uint16_t)(tier.size() + 1), tier.size() + 1);
	}

	/**
	 * Expects the status and no name from the C function, with a buffer
	 * long enough for any name and with one too short for any.
	 */
	void expectFailure(
		const char *path,
		fiftyoneDegreesStatusCode expected,
		const std::string &where) {
		ProductResult result = getProduct(path, productNameBytes);
		EXPECT_EQ(expected, result.status) << where;
		EXPECT_EQ("", result.name) << where;
		EXPECT_EQ((size_t)0, result.needed) << where;
		result = getProduct(path, 1);
		EXPECT_EQ(expected, result.status) << where << ", short buffer";
		EXPECT_EQ("", result.name) << where << ", short buffer";
		EXPECT_EQ((size_t)0, result.needed) << where << ", short buffer";
		EXPECT_EQ(expected, getEngineStatus(path)) << where << ", engine";
	}
};

/**
 * Test that the product name of each data file found is the prefix followed
 * by the tier an engine that loaded the same file gives, from both the C
 * function and the engine.
 */
TEST_F(HashProductTests, FromFile_EachDataFile_MatchesTheLoadedEngine) {
	int found = 0;
	for (int i = 0; i < _HashFileNamesLength; i++) {
		const std::string path = GetFilePath(
			_dataFolderName,
			_HashFileNames[i]);
		if (path.empty()) {
			continue;
		}
		found++;
		FiftyoneDegrees::DeviceDetection::Hash::ConfigHash config;
		FiftyoneDegrees::Common::RequiredPropertiesConfig properties(
			"IsMobile");
		EngineHash engine(path, &config, &properties);
		const std::string tier = engine.getProduct();
		EXPECT_FALSE(tier.empty()) << _HashFileNames[i];
		const std::string expected = expectedProduct(tier);
		const ProductResult result = getProduct(
			path.c_str(),
			productNameBytes);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, result.status) <<
			_HashFileNames[i];
		EXPECT_EQ(expected, result.name) << _HashFileNames[i];
		EXPECT_EQ(expected.size() + 1, result.needed) << _HashFileNames[i];
		EXPECT_EQ(expected, EngineHash::getProductFromFile(path)) <<
			_HashFileNames[i];
	}
	EXPECT_LT(0, found) << "No data file was found.";
}

/**
 * Test that each tier 51Degrees publishes a Hash data file for gives the
 * name the Distributor knows the product by.
 */
TEST_F(HashProductTests, FromFile_KnownTiers) {
	const char* const tiers[][2] = {
		{ "TAC", "V4TAC" },
		{ "Enterprise", "V4Enterprise" },
		{ "Lite", "V4Lite" } };
	for (const auto &tier : tiers) {
		ASSERT_NO_FATAL_FAILURE(writeTier(tier[0]));
		const ProductResult result = getProduct(
			productFileName,
			productNameBytes);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, result.status) << tier[0];
		EXPECT_EQ(tier[1], result.name) << tier[0];
		EXPECT_EQ(std::string(tier[1]),
			EngineHash::getProductFromFile(productFileName)) << tier[0];
	}
}

/**
 * Test that a tier that already starts with the prefix, in any case, is the
 * product name as it is, and that a tier shorter than the prefix, or one
 * that only starts like it, is given the prefix.
 */
TEST_F(HashProductTests, FromFile_TierStartingWithThePrefix) {
	const char* const tiers[][2] = {
		{ "V4TAC", "V4TAC" },
		{ "v4Custom", "v4Custom" },
		{ "V", "V4V" },
		{ "V5", "V4V5" },
		{ "4V", "V44V" } };
	for (const auto &tier : tiers) {
		ASSERT_NO_FATAL_FAILURE(writeTier(tier[0]));
		const ProductResult result = getProduct(
			productFileName,
			productNameBytes);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, result.status) << tier[0];
		EXPECT_EQ(tier[1], result.name) << tier[0];
		EXPECT_EQ(strlen(tier[1]) + 1, result.needed) << tier[0];
	}
}

/**
 * Test that a buffer one byte too short gives the bytes needed and an empty
 * string, that no buffer gives the bytes needed, and that a buffer of
 * exactly the bytes needed gives the name.
 */
TEST_F(HashProductTests, FromFile_BufferLength) {
	ASSERT_NO_FATAL_FAILURE(writeTier("TAC"));
	const size_t needed = strlen("V4TAC") + 1;
	ProductResult result = getProduct(productFileName, needed - 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY, result.status);
	EXPECT_EQ("", result.name);
	EXPECT_EQ(needed, result.needed);
	result = getProduct(productFileName, 0);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY, result.status);
	EXPECT_EQ(needed, result.needed);
	result = getProduct(productFileName, needed);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, result.status);
	EXPECT_EQ("V4TAC", result.name);
	EXPECT_EQ(needed, result.needed);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INSUFFICIENT_CAPACITY,
		fiftyoneDegreesHashGetProductFromFile(
			productFileName,
			nullptr,
			0,
			nullptr)) << "needed may be null";
}

/**
 * Test that a null buffer with a length is refused.
 */
TEST_F(HashProductTests, FromFile_NullBuffer) {
	ASSERT_NO_FATAL_FAILURE(writeTier("TAC"));
	size_t needed = 12345;
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_NULL_POINTER,
		fiftyoneDegreesHashGetProductFromFile(
			productFileName,
			nullptr,
			productNameBytes,
			&needed));
	EXPECT_EQ((size_t)0, needed);
}

/**
 * Test that the engine gives a name longer than the bytes it tries first.
 */
TEST_F(HashProductTests, Engine_FromFile_LongTier) {
	const std::string tier(200, 'T');
	ASSERT_NO_FATAL_FAILURE(writeTier(tier));
	EXPECT_EQ(productPrefix + tier,
		EngineHash::getProductFromFile(productFileName));
}

/**
 * Test that a file that does not exist gives its status and no name.
 */
TEST_F(HashProductTests, FromFile_MissingFile) {
	expectFailure(
		"HashProductTests-NoSuchFile.hash",
		FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND,
		"a file that does not exist");
}

/**
 * Test that a file shorter than the header is corrupt.
 */
TEST_F(HashProductTests, FromFile_ShorterThanTheHeader) {
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesFileWrite(
			productFileName,
			header.data(),
			header.size() - 1));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"a file shorter than the header");
}

/**
 * Test that a file that ends before the name does is corrupt, wherever in
 * the name it ends, and that a name with no terminating null is corrupt.
 */
TEST_F(HashProductTests, FromFile_EndsBeforeTheName) {
	ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesFileWrite(
			productFileName,
			header.data(),
			header.size()));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"the header alone");
	ASSERT_NO_FATAL_FAILURE(writeFile("Enterprise", 11, 0));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"the size alone");
	ASSERT_NO_FATAL_FAILURE(writeFile("Enterprise", 11, 1));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"one character");
	ASSERT_NO_FATAL_FAILURE(writeFile("Enterprise", 11, 10));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"no terminating null");
	ASSERT_NO_FATAL_FAILURE(writeFile("Enterprise", 10, 10));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"a size that leaves out the terminating null");
	ASSERT_NO_FATAL_FAILURE(writeTier("TAC"));
	ASSERT_NO_FATAL_FAILURE(setWrittenNameOffset(1000));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"a name offset past the end of the file");
}

/**
 * Test that an empty tier, or a negative name offset, is corrupt, as no
 * product name is known.
 */
TEST_F(HashProductTests, FromFile_NoTier) {
	ASSERT_NO_FATAL_FAILURE(writeTier(""));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"an empty tier");
	ASSERT_NO_FATAL_FAILURE(writeFile("", 0, 0));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"a size of zero");
	ASSERT_NO_FATAL_FAILURE(writeTier("TAC"));
	ASSERT_NO_FATAL_FAILURE(setWrittenNameOffset(-1));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		"a negative name offset");
}

/**
 * Test that each IP Intelligence data file found is not read as a Hash data
 * file, and nor is a Hash header carrying the IP Intelligence version, which
 * runs where no IP Intelligence data file is present.
 */
TEST_F(HashProductTests, FromFile_IpiFile) {
	for (const char *name : ipiFileNames) {
		const std::string path = GetFilePath(ipiDataFolderName, name);
		if (path.empty() == false) {
			expectFailure(
				path.c_str(),
				FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
				name);
		}
	}
	setInt32(
		offsetof(fiftyoneDegreesDataSetHashHeader, versionMajor),
		ipiVersionMajor);
	setInt32(
		offsetof(fiftyoneDegreesDataSetHashHeader, versionMinor),
		ipiVersionMinor);
	ASSERT_NO_FATAL_FAILURE(writeTier("TAC"));
	expectFailure(
		productFileName,
		FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
		"the IP Intelligence version");
}
