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

#ifndef FIFTYONE_DEGREES_IP_H_INCLUDED
#define FIFTYONE_DEGREES_IP_H_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

/**
 * @ingroup FiftyOneDegreesCommon
 * @defgroup fiftyoneDegreesIp IP
 *
 * Types and methods to parse IP address strings.
 *
 * ## Introduction
 *
 * IP v4 and v6 addresses can be parsed using the
 * #fiftyoneDegreesIpAddressParse method.
 *
 * @{
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "data.h"
#include "common.h"

/**
 * The number of bytes in an Ipv4 Address
 */
#define FIFTYONE_DEGREES_IPV4_LENGTH 4

/**
 * The number of bytes in an Ipv6 Address
 */
#define FIFTYONE_DEGREES_IPV6_LENGTH 16

/**
 * Enum indicating the type of IP address.
 */
typedef enum e_fiftyone_degrees_ip_type {
	FIFTYONE_DEGREES_IP_TYPE_INVALID = 0, /**< Invalid IP address */
	FIFTYONE_DEGREES_IP_TYPE_IPV4 = 4, /**< An IPv4 address */
	FIFTYONE_DEGREES_IP_TYPE_IPV6 = 6, /**< An IPv6 address */
} fiftyoneDegreesIpType;

/**
 * The structure to hold a IP Address in byte array format.
 */
typedef struct fiftyone_degrees_ip_address_t {
	byte value[FIFTYONE_DEGREES_IPV6_LENGTH]; /**< Buffer to hold the IP 
											  address bytes array. */
	byte type; /**< The type of the IP. @see fiftyoneDegreesIpType */
} fiftyoneDegreesIpAddress;

/**
 * Parse a single IP address string. The string is read only when all of it is
 * one of these forms:
 * - an IPv4 address in dotted-decimal form, such as 203.0.113.1, with no
 *   leading zeros
 * - an IPv6 address in any text form of RFC 4291 section 2.2, such as
 *   2001:db8::1 or ::ffff:203.0.113.1
 * - an IPv4 address, a colon and a port, such as 203.0.113.1:8080
 * - an IPv6 address in square brackets, with or without a colon and a port
 *   after the closing bracket, such as [2001:db8::1]:8080
 *
 * The port is checked and then dropped. Anything else is not read, including a
 * prefix length, text or a space before or after the address, a list of
 * addresses and an IPv6 zone index. The address is not changed in that case.
 * An IPv4-mapped IPv6 address stays an IPv6 address here.
 * See
 * https://github.com/51Degrees/specifications/blob/main/ip-intelligence-specification/pipeline-elements/ip-intelligence-on-premise.md#reading-the-client-ip-value
 *
 * Does not modify the last 12 (out of 16) bytes when parsing IPv4.
 * @param start of the string containing the IP address to parse
 * @param end the character after the last character of the string
 * @param address memory to write parsed IP address into.
 * @return <c>true</c> if address was parsed correctly, <c>false</c> otherwise
 */
EXTERNAL bool fiftyoneDegreesIpAddressParse(
	const char *start,
	const char *end,
	fiftyoneDegreesIpAddress *address);

/**
 * Compare two IP addresses in its binary form
 * @param ipAddress1 the first IP address
 * @param ipAddress2 the second IP address
 * @param type the type of IP address. This determine
 * the number of bytes to compare. IPv4 require 4 bytes
 * and IPv6 require 16 bytes
 * @return a value indicate the result:
 * 0 for equals
 * > 0 for ipAddress1 comes after ipAddress2
 * < 0 for ipAddress1 comes before ipAddress2
 */
EXTERNAL int fiftyoneDegreesIpAddressesCompare(
	const unsigned char *ipAddress1,
	const unsigned char *ipAddress2,
	fiftyoneDegreesIpType type);

/**
 * @}
 */

#endif
