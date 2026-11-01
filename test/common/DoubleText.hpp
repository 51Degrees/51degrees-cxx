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

#ifndef FIFTYONE_DEGREES_TEST_DOUBLE_TEXT_HPP
#define FIFTYONE_DEGREES_TEST_DOUBLE_TEXT_HPP

/**
 * @file DoubleText.hpp
 * @brief The text of a double from the string builder, and from a copy of the
 * method that works out one decimal place at a time, which the tests compare.
 */

#include <cstdint>
#include <string>
#include "../../src/common/fiftyone.h"

/**
 * A copy of the method the string builder uses for the values it does not
 * write from the scaled integer, which gives the text the tests expect.
 */
inline void doubleTextByPlaceAdd(
	fiftyoneDegreesStringBuilder * const builder,
	const double value,
	const uint8_t decimalPlaces) {
	bool addNegative = false;
	const int digitPlaces = MAX_DOUBLE_DECIMAL_PLACES < decimalPlaces
		? MAX_DOUBLE_DECIMAL_PLACES : decimalPlaces;
	int remDigits = digitPlaces;

	int intPart = (int)value;
	double fracPart = value - intPart;

	if (fracPart < 0) {
		if (intPart == 0) {
			addNegative = true;
		}
		fracPart = -fracPart;
	}

	if (!fracPart || remDigits <= 0) {
		if (fracPart >= 0.5) {
			if (value < 0) {
				intPart--;
			} else {
				intPart++;
			}
		}
		fiftyoneDegreesStringBuilderAddInteger(builder, intPart);
		return;
	}

	char floatTail[MAX_DOUBLE_DECIMAL_PLACES + 2];
	floatTail[0] = '.';
	char *digits = floatTail + 1;

	char *nextDigit = digits;
	while (remDigits > 0 && fracPart) {
		remDigits--;
		fracPart *= 10;
		*nextDigit = (char)fracPart;
		fracPart -= *nextDigit;
		if (!remDigits && fracPart >= 0.5) {
			while (nextDigit > floatTail && *nextDigit == 9) {
				--nextDigit;
				++remDigits;
			}
			if (nextDigit > floatTail) {
				(*nextDigit)++;
				++nextDigit;
				break;
			} else {
				if (value < 0) {
					intPart--;
				} else {
					intPart++;
				}
				fiftyoneDegreesStringBuilderAddInteger(builder, intPart);
				return;
			}
		}
		++nextDigit;
	}
	*nextDigit = '\0';

	int digitsToAdd = digitPlaces - remDigits;
	for (nextDigit = digits + digitsToAdd - 1;
		nextDigit >= digits;
		--nextDigit) {
		if (*nextDigit) {
			break;
		}
		--digitsToAdd;
	}
	if (digitsToAdd <= 0) {
		fiftyoneDegreesStringBuilderAddInteger(builder, intPart);
		return;
	}
	for (; nextDigit >= digits; --nextDigit) {
		*nextDigit += '0';
	}
	if (addNegative) {
		fiftyoneDegreesStringBuilderAddChar(builder, '-');
	}
	fiftyoneDegreesStringBuilderAddInteger(builder, intPart);
	fiftyoneDegreesStringBuilderAddChars(
		builder,
		floatTail,
		(size_t)digitsToAdd + 1);
}

/**
 * The text of the value from the string builder.
 */
inline std::string doubleText(
	const double value,
	const uint8_t decimalPlaces) {
	char buffer[64];
	fiftyoneDegreesStringBuilder builder = { buffer, sizeof(buffer) };
	fiftyoneDegreesStringBuilderInit(&builder);
	fiftyoneDegreesStringBuilderAddDouble(&builder, value, decimalPlaces);
	fiftyoneDegreesStringBuilderComplete(&builder);
	return std::string(buffer);
}

/**
 * The text of the value worked out one decimal place at a time.
 */
inline std::string doubleTextByPlace(
	const double value,
	const uint8_t decimalPlaces) {
	char buffer[64];
	fiftyoneDegreesStringBuilder builder = { buffer, sizeof(buffer) };
	fiftyoneDegreesStringBuilderInit(&builder);
	doubleTextByPlaceAdd(&builder, value, decimalPlaces);
	fiftyoneDegreesStringBuilderComplete(&builder);
	return std::string(buffer);
}

#endif
