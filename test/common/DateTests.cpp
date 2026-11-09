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
 * @file DateTests.cpp
 * @brief Tests which dates fiftyoneDegreesDateIsValid accepts, being the days
 * of the Gregorian calendar from year 1.
 */

#include "pch.h"
#include <cstdint>
#include "../../src/common/date.h"

static fiftyoneDegreesDate makeDate(int year, int month, int day) {
	fiftyoneDegreesDate date;
	date.year = (int16_t)year;
	date.month = (uint8_t)month;
	date.day = (uint8_t)day;
	return date;
}

static bool isValid(int year, int month, int day) {
	const fiftyoneDegreesDate date = makeDate(year, month, day);
	return fiftyoneDegreesDateIsValid(&date);
}

/**
 * Test that the date occupies the four bytes a data file stores it in.
 */
TEST(Date, Size_IsFourBytes) {
	EXPECT_EQ((size_t)4, sizeof(fiftyoneDegreesDate));
}

/**
 * Test that an ordinary date, the first day of year 1 and the last day of the
 * largest year the date can hold are valid.
 */
TEST(Date, Valid_OrdinaryAndExtremeDates) {
	EXPECT_TRUE(isValid(2026, 11, 7));
	EXPECT_TRUE(isValid(1, 1, 1));
	EXPECT_TRUE(isValid(INT16_MAX, 12, 31));
}

/**
 * Test that the last day of every month in a year that is not a leap year is
 * valid, and the day after it is not.
 */
TEST(Date, LastDayOfEachMonth_IsValid_DayAfterIsNot) {
	const int lastDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	for (int month = 1; month <= 12; month++) {
		EXPECT_TRUE(isValid(2023, month, lastDays[month - 1])) <<
			"Month " << month;
		EXPECT_FALSE(isValid(2023, month, lastDays[month - 1] + 1)) <<
			"Month " << month;
	}
}

/**
 * Test that February 29 is valid in a year divisible by 4, and in a century
 * year divisible by 400, but not in another century year or in a year not
 * divisible by 4, and that February 30 is never valid.
 */
TEST(Date, LeapDay_IsValidOnlyInALeapYear) {
	EXPECT_TRUE(isValid(2024, 2, 29));
	EXPECT_TRUE(isValid(2000, 2, 29));
	EXPECT_FALSE(isValid(1900, 2, 29));
	EXPECT_FALSE(isValid(2023, 2, 29));
	EXPECT_FALSE(isValid(2024, 2, 30));
}

/**
 * Test that a year of zero or less is not valid, whatever the month and day.
 */
TEST(Date, YearZeroOrLess_IsNotValid) {
	EXPECT_FALSE(isValid(0, 1, 1));
	EXPECT_FALSE(isValid(-1, 1, 1));
	EXPECT_FALSE(isValid(INT16_MIN, 1, 1));
}

/**
 * Test that a month of zero or above 12 is not valid.
 */
TEST(Date, MonthOutsideYear_IsNotValid) {
	EXPECT_FALSE(isValid(2026, 0, 1));
	EXPECT_FALSE(isValid(2026, 13, 1));
	EXPECT_FALSE(isValid(2026, UINT8_MAX, 1));
}

/**
 * Test that a day of zero or above any month's length is not valid.
 */
TEST(Date, DayOutsideMonth_IsNotValid) {
	EXPECT_FALSE(isValid(2026, 1, 0));
	EXPECT_FALSE(isValid(2026, 1, 32));
	EXPECT_FALSE(isValid(2026, 1, UINT8_MAX));
}

/**
 * Test that a date of all zeros, which a data file holds when it has no date,
 * is not valid.
 */
TEST(Date, AllZero_IsNotValid) {
	EXPECT_FALSE(isValid(0, 0, 0));
}

/**
 * Test that no date is not valid, rather than being read.
 */
TEST(Date, Null_IsNotValid) {
	EXPECT_FALSE(fiftyoneDegreesDateIsValid(nullptr));
}
