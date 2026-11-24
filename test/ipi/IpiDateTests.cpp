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
 * @file IpiDateTests.cpp
 * @brief Tests that the published and next update dates of an IP
 * Intelligence data set, from the C functions and from the engine, are the
 * dates in the header of its data file when they are valid, and a year, month
 * and day of zero when they are not.
 *
 * The tests hold a copy of the ASN data file in memory, change its dates where
 * a test needs other dates, and load the copy. CI downloads the ASN file and
 * never stands another file in for it. Nothing else in the data set reads the
 * dates, so any date loads. The test that compares the engine with each IP
 * Intelligence data file found needs only that file, so it runs without the
 * ASN file.
 */

#include "../common/Base.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../../src/ipi/ipi.h"
#include "../../src/ipi/EngineIpi.hpp"

static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

static const size_t publishedPosition =
	offsetof(fiftyoneDegreesDataSetIpiHeader, published);

static const size_t nextUpdatePosition =
	offsetof(fiftyoneDegreesDataSetIpiHeader, nextUpdate);

/**
 * Where the published date starts in every IP Intelligence data file. The
 * engine test reads the bytes there itself, so it does not rely on the header
 * struct to find the date.
 */
static const size_t publishedFileOffset = 66;

static_assert(
	offsetof(fiftyoneDegreesDataSetIpiHeader, published) ==
		publishedFileOffset,
	"The published date must be at offset 66 of the header.");

/**
 * A date that is not valid, and why.
 */
struct InvalidDate {
	int year;
	int month;
	int day;
	const char *name;
};

/**
 * Dates the header can hold that are not valid, from each rule the check
 * applies.
 */
static const InvalidDate invalidDates[] = {
	{ 0, 0, 0, "all zero" },
	{ -1, 1, 1, "year -1" },
	{ 2026, 13, 1, "month 13" },
	{ 2026, 1, 0, "day 0" },
	{ 2024, 2, 30, "February 30" },
	{ 2023, 2, 29, "February 29 in a year that is not a leap year" } };

static fiftyoneDegreesDate makeDate(int year, int month, int day) {
	fiftyoneDegreesDate date;
	date.year = (int16_t)year;
	date.month = (uint8_t)month;
	date.day = (uint8_t)day;
	return date;
}

/**
 * Both dates from a data set, and whether each was valid.
 */
struct HeaderDates {
	fiftyoneDegreesDate published;
	fiftyoneDegreesDate nextUpdate;
	bool publishedValid;
	bool nextUpdateValid;
};

static void expectDate(
	const fiftyoneDegreesDate &expected,
	const fiftyoneDegreesDate &actual,
	const std::string &where) {
	EXPECT_EQ((int)expected.year, (int)actual.year) << where;
	EXPECT_EQ((int)expected.month, (int)actual.month) << where;
	EXPECT_EQ((int)expected.day, (int)actual.day) << where;
}

static void expectDate(
	const fiftyoneDegreesDate &expected,
	const FiftyoneDegrees::Common::Date &actual,
	const std::string &where) {
	EXPECT_EQ((int)expected.year, actual.getYear()) << where;
	EXPECT_EQ((int)expected.month, actual.getMonth()) << where;
	EXPECT_EQ((int)expected.day, actual.getDay()) << where;
}

class IpiDateTests : public Base {
protected:
	std::vector<unsigned char> data;
	std::string dataFilePath;

	void SetUp() override {
		Base::SetUp();
		dataFilePath = GetFilePath(_dataFolderName, asnFileName);
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "The file '" << asnFileName << "' was not found. "
				"Run data/get-data-files.sh or .ps1 to download it.";
		}
		FILE *file = fopen(dataFilePath.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << dataFilePath << "'.";
		fseek(file, 0, SEEK_END);
		data.resize((size_t)ftell(file));
		rewind(file);
		const size_t read = fread(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), read) << "Could not read '" <<
			dataFilePath << "'.";
		ASSERT_GT(data.size(), sizeof(fiftyoneDegreesDataSetIpiHeader));
	}

	void TearDown() override {
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(data);
		std::string().swap(dataFilePath);
		Base::TearDown();
	}

	void setDate(size_t position, const fiftyoneDegreesDate &date) {
		memcpy(data.data() + position, &date, sizeof(date));
	}

	fiftyoneDegreesDate getDate(size_t position) const {
		fiftyoneDegreesDate date;
		memcpy(&date, data.data() + position, sizeof(date));
		return date;
	}

	/**
	 * Gets the dates from the C functions for a manager initialised from the
	 * data file, or from the copy in memory when path is null. Each date is
	 * set to a valid date first, so a test sees that a date which is not
	 * valid is replaced with zeros.
	 */
	void getDates(
		const fiftyoneDegreesConfigIpi &source,
		const char *path,
		HeaderDates *dates) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesConfigIpi config = source;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status = path == nullptr ?
			fiftyoneDegreesIpiInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				data.data(),
				(fiftyoneDegreesFileOffset)data.size(),
				exception) :
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path,
				exception);
		ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
		ASSERT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);
		dates->published = makeDate(1, 1, 1);
		dates->nextUpdate = makeDate(1, 1, 1);
		dates->publishedValid = fiftyoneDegreesIpiGetPublished(
			&manager,
			&dates->published);
		dates->nextUpdateValid = fiftyoneDegreesIpiGetNextUpdate(
			&manager,
			&dates->nextUpdate);
		fiftyoneDegreesResourceManagerFree(&manager);
	}

	void getDatesFromMemory(HeaderDates *dates) {
		getDates(fiftyoneDegreesIpiInMemoryConfig, nullptr, dates);
	}

	/**
	 * Gets the dates from an engine made from the copy in memory.
	 */
	void getEngineDates(
		FiftyoneDegrees::Common::Date *published,
		FiftyoneDegrees::Common::Date *nextUpdate) {
		FiftyoneDegrees::IpIntelligence::ConfigIpi config;
		FiftyoneDegrees::Common::RequiredPropertiesConfig properties;
		FiftyoneDegrees::IpIntelligence::EngineIpi engine(
			(void*)data.data(),
			(fiftyoneDegreesFileOffset)data.size(),
			&config,
			&properties);
		*published = engine.getPublishedTime();
		*nextUpdate = engine.getUpdateAvailableTime();
	}
};

/**
 * Test that the data file holds valid dates, so the tests below that rely on
 * it read real dates.
 */
TEST_F(IpiDateTests, DataFile_HoldsValidDates) {
	const fiftyoneDegreesDate published = getDate(publishedPosition);
	const fiftyoneDegreesDate nextUpdate = getDate(nextUpdatePosition);
	EXPECT_TRUE(fiftyoneDegreesDateIsValid(&published));
	EXPECT_TRUE(fiftyoneDegreesDateIsValid(&nextUpdate));
}

/**
 * Test that the C functions give the dates in the header of the data file,
 * whether the data set holds the file in memory or reads it.
 */
TEST_F(IpiDateTests, UnchangedFile_GivesTheHeaderDates) {
	const fiftyoneDegreesDate published = getDate(publishedPosition);
	const fiftyoneDegreesDate nextUpdate = getDate(nextUpdatePosition);
	HeaderDates dates = {};
	ASSERT_NO_FATAL_FAILURE(getDatesFromMemory(&dates));
	EXPECT_TRUE(dates.publishedValid);
	EXPECT_TRUE(dates.nextUpdateValid);
	expectDate(published, dates.published, "published, from memory");
	expectDate(nextUpdate, dates.nextUpdate, "next update, from memory");
	if (fiftyoneDegreesCollectionGetIsMemoryOnly() == false) {
		ASSERT_NO_FATAL_FAILURE(getDates(
			fiftyoneDegreesIpiLowMemoryConfig,
			dataFilePath.c_str(),
			&dates));
		EXPECT_TRUE(dates.publishedValid);
		EXPECT_TRUE(dates.nextUpdateValid);
		expectDate(published, dates.published, "published, from the file");
		expectDate(nextUpdate, dates.nextUpdate, "next update, from the file");
	}
}

/**
 * Test that February 29 in a leap year is given as it is.
 */
TEST_F(IpiDateTests, LeapDay_IsGiven) {
	const fiftyoneDegreesDate leapDay = makeDate(2024, 2, 29);
	setDate(publishedPosition, leapDay);
	setDate(nextUpdatePosition, leapDay);
	HeaderDates dates = {};
	ASSERT_NO_FATAL_FAILURE(getDatesFromMemory(&dates));
	EXPECT_TRUE(dates.publishedValid);
	EXPECT_TRUE(dates.nextUpdateValid);
	expectDate(leapDay, dates.published, "published");
	expectDate(leapDay, dates.nextUpdate, "next update");
}

/**
 * Test that each date that is not valid is reported as not valid and given as
 * zeros, and that one date not being valid leaves the other as it is.
 */
TEST_F(IpiDateTests, InvalidDate_IsNotValidAndGivenAsZeros) {
	const fiftyoneDegreesDate zero = makeDate(0, 0, 0);
	const fiftyoneDegreesDate published = getDate(publishedPosition);
	const fiftyoneDegreesDate nextUpdate = getDate(nextUpdatePosition);
	for (const InvalidDate &invalid : invalidDates) {
		const fiftyoneDegreesDate date =
			makeDate(invalid.year, invalid.month, invalid.day);
		HeaderDates dates = {};

		setDate(publishedPosition, date);
		setDate(nextUpdatePosition, nextUpdate);
		ASSERT_NO_FATAL_FAILURE(getDatesFromMemory(&dates));
		EXPECT_FALSE(dates.publishedValid) << invalid.name;
		expectDate(zero, dates.published, invalid.name);
		EXPECT_TRUE(dates.nextUpdateValid) << invalid.name;
		expectDate(nextUpdate, dates.nextUpdate, invalid.name);

		setDate(publishedPosition, published);
		setDate(nextUpdatePosition, date);
		ASSERT_NO_FATAL_FAILURE(getDatesFromMemory(&dates));
		EXPECT_TRUE(dates.publishedValid) << invalid.name;
		expectDate(published, dates.published, invalid.name);
		EXPECT_FALSE(dates.nextUpdateValid) << invalid.name;
		expectDate(zero, dates.nextUpdate, invalid.name);
	}
}

/**
 * Test that the engine gives the dates in the header of the data file.
 */
TEST_F(IpiDateTests, Engine_UnchangedFile_GivesTheHeaderDates) {
	FiftyoneDegrees::Common::Date published;
	FiftyoneDegrees::Common::Date nextUpdate;
	ASSERT_NO_FATAL_FAILURE(getEngineDates(&published, &nextUpdate));
	expectDate(getDate(publishedPosition), published, "published");
	expectDate(getDate(nextUpdatePosition), nextUpdate, "next update");
}

/**
 * Test that the engine gives a year, month and day of zero for each date that
 * is not valid.
 */
TEST_F(IpiDateTests, Engine_InvalidDate_GivesZeros) {
	const fiftyoneDegreesDate zero = makeDate(0, 0, 0);
	for (const InvalidDate &invalid : invalidDates) {
		const fiftyoneDegreesDate date =
			makeDate(invalid.year, invalid.month, invalid.day);
		setDate(publishedPosition, date);
		setDate(nextUpdatePosition, date);
		FiftyoneDegrees::Common::Date published;
		FiftyoneDegrees::Common::Date nextUpdate;
		ASSERT_NO_FATAL_FAILURE(getEngineDates(&published, &nextUpdate));
		expectDate(zero, published, invalid.name);
		expectDate(zero, nextUpdate, invalid.name);
	}
}

/**
 * Writes the first length bytes of the data file copy to a file of the given
 * name, for the tests that read the published date from a file.
 */
static void writeFile(
	const char *path,
	const std::vector<unsigned char> &bytes,
	size_t length) {
	FILE *file = fopen(path, "wb");
	ASSERT_NE(nullptr, file) << "Could not create '" << path << "'.";
	const size_t written = fwrite(bytes.data(), 1, length, file);
	fclose(file);
	ASSERT_EQ(length, written) << "Could not write '" << path << "'.";
}

/** A file the tests below write and remove. */
static const char fromFileName[] = "IpiDateTests-FromFile.ipi";

/**
 * Test that the published date read from a data file without loading it is
 * the date in its header.
 */
TEST_F(IpiDateTests, FromFile_GivesThePublishedDate) {
	fiftyoneDegreesDate date = makeDate(1, 1, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiGetPublishedFromFile(dataFilePath.c_str(), &date));
	expectDate(getDate(publishedPosition), date, "published, from the file");
}

/**
 * Test that only the header is read, as a file holding nothing but the
 * header of the data file gives the same date.
 */
TEST_F(IpiDateTests, FromFile_ReadsOnlyTheHeader) {
	ASSERT_NO_FATAL_FAILURE(writeFile(
		fromFileName,
		data,
		sizeof(fiftyoneDegreesDataSetIpiHeader)));
	fiftyoneDegreesDate date = makeDate(1, 1, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiGetPublishedFromFile(fromFileName, &date));
	expectDate(getDate(publishedPosition), date, "published, from the header");
	remove(fromFileName);
}

/**
 * Test that a file that does not exist gives its status and a zero date.
 */
TEST_F(IpiDateTests, FromFile_MissingFile) {
	fiftyoneDegreesDate date = makeDate(1, 1, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND,
		fiftyoneDegreesIpiGetPublishedFromFile(
			"IpiDateTests-NoSuchFile.ipi",
			&date));
	expectDate(makeDate(0, 0, 0), date, "a file that does not exist");
}

/**
 * Test that a file shorter than the header is corrupt and gives a zero date.
 */
TEST_F(IpiDateTests, FromFile_ShorterThanTheHeader) {
	ASSERT_NO_FATAL_FAILURE(writeFile(
		fromFileName,
		data,
		sizeof(fiftyoneDegreesDataSetIpiHeader) - 1));
	fiftyoneDegreesDate date = makeDate(1, 1, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		fiftyoneDegreesIpiGetPublishedFromFile(fromFileName, &date));
	expectDate(makeDate(0, 0, 0), date, "a file shorter than the header");
	remove(fromFileName);
}

/**
 * Test that a file of another version is not read as an IP Intelligence data file.
 */
TEST_F(IpiDateTests, FromFile_OtherVersion) {
	const int32_t major = 3;
	memcpy(
		data.data() + offsetof(fiftyoneDegreesDataSetIpiHeader, versionMajor),
		&major,
		sizeof(major));
	ASSERT_NO_FATAL_FAILURE(writeFile(
		fromFileName,
		data,
		sizeof(fiftyoneDegreesDataSetIpiHeader)));
	fiftyoneDegreesDate date = makeDate(1, 1, 1);
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_INCORRECT_VERSION,
		fiftyoneDegreesIpiGetPublishedFromFile(fromFileName, &date));
	expectDate(makeDate(0, 0, 0), date, "a file of version 3");
	remove(fromFileName);
}

/**
 * Test that a header whose published date is not valid is corrupt and gives
 * a zero date, so a caller never compares an invalid date.
 */
TEST_F(IpiDateTests, FromFile_InvalidDate) {
	for (const InvalidDate &invalid : invalidDates) {
		setDate(
			publishedPosition,
			makeDate(invalid.year, invalid.month, invalid.day));
		ASSERT_NO_FATAL_FAILURE(writeFile(
			fromFileName,
			data,
			sizeof(fiftyoneDegreesDataSetIpiHeader)));
		fiftyoneDegreesDate date = makeDate(1, 1, 1);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
			fiftyoneDegreesIpiGetPublishedFromFile(fromFileName, &date)) <<
			invalid.name;
		expectDate(makeDate(0, 0, 0), date, invalid.name);
		remove(fromFileName);
	}
}

/**
 * Gets the status the engine throws for a file that gives no published date,
 * or fails the test when it gives a date instead.
 */
static fiftyoneDegreesStatusCode getEngineFromFileStatus(const char *path) {
	try {
		FiftyoneDegrees::IpIntelligence::EngineIpi::getPublishedTimeFromFile(
			path);
	}
	catch (const FiftyoneDegrees::Common::StatusCodeException &e) {
		return e.getCode();
	}
	ADD_FAILURE() << "A date was given for '" << path << "'.";
	return FIFTYONE_DEGREES_STATUS_SUCCESS;
}

/**
 * Reads the published date from the bytes of a data file at its fixed offset,
 * rather than through the header struct. The year is a 16 bit little-endian
 * integer, followed by a byte for the month and a byte for the day.
 */
static void readPublishedBytes(
	const std::string &path,
	fiftyoneDegreesDate *date) {
	unsigned char bytes[4];
	FILE *file = fopen(path.c_str(), "rb");
	ASSERT_NE(nullptr, file) << "Could not open '" << path << "'.";
	size_t read = 0;
	if (fseek(file, (long)publishedFileOffset, SEEK_SET) == 0) {
		read = fread(bytes, 1, sizeof(bytes), file);
	}
	fclose(file);
	ASSERT_EQ(sizeof(bytes), read) << "Could not read '" << path << "'.";
	int16_t year;
	memcpy(&year, bytes, sizeof(year));
	*date = makeDate(year, bytes[2], bytes[3]);
}

/**
 * Tests that need only an IP Intelligence data file, the Lite or Enterprise
 * one, and not the ASN file.
 */
class IpiDataFileDateTests : public Base {};

/**
 * Test that the engine's date read from each data file found is the date in
 * the bytes of its header, and the date an engine that loaded the same file
 * gives.
 */
TEST_F(IpiDataFileDateTests, Engine_FromFile_MatchesTheLoadedEngine) {
	int found = 0;
	for (int i = 0; i < _IpiFileNamesLength; i++) {
		const std::string path = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		if (path.empty()) {
			continue;
		}
		found++;
		const FiftyoneDegrees::Common::Date fromFile =
			FiftyoneDegrees::IpIntelligence::EngineIpi::
			getPublishedTimeFromFile(path);
		fiftyoneDegreesDate onDisk = makeDate(1, 1, 1);
		ASSERT_NO_FATAL_FAILURE(readPublishedBytes(path, &onDisk));
		expectDate(onDisk, fromFile, _IpiFileNames[i]);
		FiftyoneDegrees::IpIntelligence::ConfigIpi config;
		FiftyoneDegrees::Common::RequiredPropertiesConfig properties;
		FiftyoneDegrees::IpIntelligence::EngineIpi engine(
			path,
			&config,
			&properties);
		const FiftyoneDegrees::Common::Date loaded =
			engine.getPublishedTime();
		EXPECT_NE(0, fromFile.getYear()) << _IpiFileNames[i];
		EXPECT_EQ(loaded.getYear(), fromFile.getYear()) << _IpiFileNames[i];
		EXPECT_EQ(loaded.getMonth(), fromFile.getMonth()) << _IpiFileNames[i];
		EXPECT_EQ(loaded.getDay(), fromFile.getDay()) << _IpiFileNames[i];
	}
	EXPECT_LT(0, found) << "No data file was found.";
}

/**
 * Test that the engine throws the status for a file that does not exist
 * rather than giving a date.
 */
TEST_F(IpiDateTests, Engine_FromFile_MissingFile) {
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND,
		getEngineFromFileStatus("IpiDateTests-NoSuchFile.ipi"));
}

/**
 * Test that the engine throws the status for a file shorter than the header
 * rather than giving a date.
 */
TEST_F(IpiDateTests, Engine_FromFile_ShorterThanTheHeader) {
	ASSERT_NO_FATAL_FAILURE(writeFile(
		fromFileName,
		data,
		sizeof(fiftyoneDegreesDataSetIpiHeader) - 1));
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		getEngineFromFileStatus(fromFileName));
	remove(fromFileName);
}

/**
 * Test that the engine throws the corrupt data status for a file whose
 * published date is zero rather than giving a date.
 */
TEST_F(IpiDateTests, Engine_FromFile_ZeroDate) {
	setDate(publishedPosition, makeDate(0, 0, 0));
	ASSERT_NO_FATAL_FAILURE(writeFile(
		fromFileName,
		data,
		sizeof(fiftyoneDegreesDataSetIpiHeader)));
	EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		getEngineFromFileStatus(fromFileName));
	remove(fromFileName);
}
