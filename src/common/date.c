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

#include "date.h"

#define MONTHS_IN_YEAR 12
#define FEBRUARY 2

/**
 * The number of days in each month of a year that is not a leap year.
 */
static const byte daysInMonth[MONTHS_IN_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * A Gregorian leap year is divisible by 4, except a century year, which has to
 * be divisible by 400.
 */
static bool isLeapYear(int year) {
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool fiftyoneDegreesDateIsValid(const fiftyoneDegreesDate *date) {
	int days;
	if (date == NULL ||
		date->year <= 0 ||
		date->month == 0 ||
		date->month > MONTHS_IN_YEAR ||
		date->day == 0) {
		return false;
	}
	days = daysInMonth[date->month - 1];
	if (date->month == FEBRUARY && isLeapYear(date->year)) {
		days++;
	}
	return date->day <= days;
}
