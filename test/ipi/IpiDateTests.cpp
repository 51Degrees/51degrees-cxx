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
 * Each test changes the dates in a copy of the ASN data file, which CI
 * downloads and never stands another file in for, and loads the copy from
 * memory. Nothing else in the data set reads the dates, so any date loads.
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
