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

#include "ip.h"
#include "fiftyone.h"
#include <string.h>

/** The largest port number, the largest value of 16 bits. */
#define MAX_PORT 65535

/** The most decimal digits in one part of an IPv4 address. */
#define MAX_IPV4_PART_DIGITS 3

/** The most hexadecimal digits in one group of an IPv6 address. */
#define MAX_IPV6_GROUP_DIGITS 4

/** The number of bytes in one group of an IPv6 address. */
#define IPV6_GROUP_LENGTH 2

static int getHexValue(const char c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	}
	if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	}
	if ('A' <= c && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/**
 * Reads an IPv4 address in dotted-decimal form. The address must fill the
 * range from current to end, and no part may have a leading zero.
 */
static bool readIpv4(
	const char *current,
	const char * const end,
	byte * const value) {
	byte parts[IPV4_LENGTH];
	for (int i = 0; i < IPV4_LENGTH; i++) {
		const char * const partStart = current;
		int part = 0;
		while (current < end && '0' <= *current && *current <= '9') {
			part = part * 10 + (*current - '0');
			current++;
			if (current - partStart > MAX_IPV4_PART_DIGITS) {
				return false;
			}
		}
		const int digits = (int)(current - partStart);
		if (digits == 0 ||
			(digits > 1 && *partStart == '0') ||
			part > UINT8_MAX) {
			return false;
		}
		parts[i] = (byte)part;
		if (i < IPV4_LENGTH - 1) {
			if (current >= end || *current != '.') {
				return false;
			}
			current++;
		}
	}
	if (current != end) {
		return false;
	}
	memcpy(value, parts, IPV4_LENGTH);
	return true;
}

/**
 * Reads an IPv6 address in any of the text forms of RFC 4291 section 2.2. The
 * address must fill the range from current to end.
 */
static bool readIpv6(
	const char *current,
	const char * const end,
	byte * const value) {
	byte bytes[IPV6_LENGTH] = { 0 };
	// The number of bytes read, and the number of bytes before the "::".
	int count = 0;
	int gap = -1;

	if (current < end && *current == ':') {
		if (end - current < 2 || current[1] != ':') {
			return false;
		}
		gap = 0;
		current += 2;
	}
	while (current < end) {
		const char * const groupStart = current;
		int group = 0;
		while (current < end && getHexValue(*current) >= 0) {
			if (current - groupStart == MAX_IPV6_GROUP_DIGITS) {
				return false;
			}
			group = group * 16 + getHexValue(*current);
			current++;
		}
		if (current == groupStart) {
			return false;
		}
		if (current < end && *current == '.') {
			// An IPv4 address is the last 32 bits.
			if (count + IPV4_LENGTH > IPV6_LENGTH ||
				readIpv4(groupStart, end, bytes + count) == false) {
				return false;
			}
			count += IPV4_LENGTH;
			break;
		}
		if (count + IPV6_GROUP_LENGTH > IPV6_LENGTH) {
			return false;
		}
		bytes[count++] = (byte)(group >> 8);
		bytes[count++] = (byte)(group & 0xFF);
		if (current == end) {
			break;
		}
		if (*current != ':') {
			return false;
		}
		current++;
		if (current == end) {
			// A single colon at the end.
			return false;
		}
		if (*current == ':') {
			if (gap >= 0) {
				return false;
			}
			gap = count;
			current++;
		}
	}

	if (gap < 0) {
		if (count != IPV6_LENGTH) {
			return false;
		}
	}
	else {
		// The "::" stands for at least one group of zeros.
		if (count > IPV6_LENGTH - IPV6_GROUP_LENGTH) {
			return false;
		}
		const int tail = count - gap;
		memmove(bytes + IPV6_LENGTH - tail, bytes + gap, (size_t)tail);
		memset(bytes + gap, 0, (size_t)(IPV6_LENGTH - count));
	}
	memcpy(value, bytes, IPV6_LENGTH);
	return true;
}

/**
 * True when the range from current to end is a port, being decimal digits
 * with a value up to MAX_PORT.
 */
static bool isPort(const char *current, const char * const end) {
	if (current >= end) {
		return false;
	}
	int port = 0;
	for (; current < end; current++) {
		if (*current < '0' || *current > '9') {
			return false;
		}
		port = port * 10 + (*current - '0');
		if (port > MAX_PORT) {
			return false;
		}
	}
	return true;
}

bool fiftyoneDegreesIpAddressParse(
	const char * const start,
	const char * const end,
	IpAddress * const address) {

	if (start == NULL || end == NULL || start >= end || address == NULL) {
		return false;
	}

	byte value[IPV6_LENGTH];
	IpType type;
	const size_t length = (size_t)(end - start);

	if (*start == '[') {
		// An IPv6 address in brackets, followed by a port or nothing.
		const char * const bracket = memchr(start, ']', length);
		if (bracket == NULL ||
			(bracket + 1 < end &&
				(bracket[1] != ':' || isPort(bracket + 2, end) == false)) ||
			readIpv6(start + 1, bracket, value) == false) {
			return false;
		}
		type = IP_TYPE_IPV6;
	}
	else {
		const char * const colon = memchr(start, ':', length);
		if (colon == NULL) {
			// Only an IPv4 address has no colon.
			if (readIpv4(start, end, value) == false) {
				return false;
			}
			type = IP_TYPE_IPV4;
		}
		else if (memchr(start, '.', (size_t)(colon - start)) != NULL) {
			// A dot before the first colon means an IPv4 address and a port.
			if (readIpv4(start, colon, value) == false ||
				isPort(colon + 1, end) == false) {
				return false;
			}
			type = IP_TYPE_IPV4;
		}
		else {
			if (readIpv6(start, end, value) == false) {
				return false;
			}
			type = IP_TYPE_IPV6;
		}
	}

	memcpy(
		address->value,
		value,
		type == IP_TYPE_IPV4 ? IPV4_LENGTH : IPV6_LENGTH);
	address->type = (byte)type;
	return true;
}

int fiftyoneDegreesIpAddressesCompare(
	const unsigned char * const ipAddress1,
	const unsigned char * const ipAddress2,
	IpType type) {
	uint16_t compareSize = 0;
	int result = 0;
	switch(type) {
	case IP_TYPE_IPV4:
		compareSize = IPV4_LENGTH;
		break;
	case IP_TYPE_IPV6:
		compareSize = IPV6_LENGTH;
		break;
	case IP_TYPE_INVALID:
	default:
		compareSize = 0;
		break;
	}

	for (uint16_t i = 0; i < compareSize; i++) {
		result = ipAddress1[i] - ipAddress2[i];
		if (result != 0) return result;
	}
	return result;
}
