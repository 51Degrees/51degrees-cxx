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
 
#include "pch.h"
#include "../../src/common/ip.h"
#include "../../src/common/fiftyone.h"
#include <memory>
#include <vector>

static bool CheckResult(
	const byte* result,
	const byte* expected,
	const uint16_t size) {
	bool match = true;
	for (uint16_t i = 0; i < size && match; i++) {
		match = (*result == *expected);
		result++;
		expected++;
	}
	return match;
}

static bool parseIpAddressInPlace(const std::unique_ptr<IpAddress> &ipAddress, const char * const ipString, const size_t length) {
	const char * const end = ipString ? ipString + length : nullptr;
	return IpAddressParse(ipString, end, ipAddress.get());
}

static std::unique_ptr<IpAddress> parseIpAddressStringOfLength(const char * const ipString, const size_t length) {
	std::unique_ptr<IpAddress> ipPtr = std::make_unique<IpAddress>();
	for (size_t i = 0; i < std::size(ipPtr->value); i++) {
		ipPtr->value[i] = static_cast<byte>(0xA0 + i);
	}
	const bool parsed =parseIpAddressInPlace(ipPtr, ipString, length);
	return parsed ? std::move(ipPtr) : nullptr;
}

static std::unique_ptr<IpAddress> parseIpAddressString(const char * const ipString) {
	return parseIpAddressStringOfLength(ipString, ipString ? strlen(ipString) : 0);
}

 // ------------------------------------------------------------------------------
 // IPv4 tests
 // ------------------------------------------------------------------------------
TEST(ParseIp, ParseIp_Ipv4_Low)
{
	const char* ip = "0.0.0.0";
	auto const result = parseIpAddressString(ip);
	byte expected[] = { 0, 0, 0, 0 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '0.0.0.0'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_High)
{
	auto const result = parseIpAddressString("255.255.255.255");
	byte expected[] = { 255, 255, 255, 255 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '255.255.255.255'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_Endline)
{
	auto const result = parseIpAddressString("0.0.0.0\n");
	EXPECT_FALSE(result) <<
		L"Expected a line ending after the address to stop it being read.";
}
TEST(ParseIp, ParseIp_Ipv4_NoOverwrite12)
{
	const char ip[] = "73.79.83.89";
	constexpr byte initial[] {
		67, 13, 43, 47, 53, 29, 61, 19, 41, 31, 59, 71, 23, 37, 17, 11,
	}, expected[] {
		73, 79, 83, 89, 53, 29, 61, 19, 41, 31, 59, 71, 23, 37, 17, 11,
	};
	std::unique_ptr<IpAddress> ipAddress = std::make_unique<IpAddress>();
	memcpy(ipAddress->value, initial, sizeof(initial));
	const bool parsed = parseIpAddressInPlace(ipAddress, ip, sizeof(ip) - 1);
	EXPECT_TRUE(parsed) <<
		L"Expected result to be non-NULL.";
	EXPECT_TRUE(CheckResult(ipAddress->value, expected, sizeof(expected) - 1)) <<
		L"Expected result to be '73.79.83.89' with the rest (12 bytes) unchanged.";
}
TEST(ParseIp, ParseIp_Ipv4_Trim0)
{
	const char* ip = "192.168.101.217";
	auto const result = parseIpAddressStringOfLength(ip, strlen(ip));
	byte expected[] = { 192, 168, 101, 217 };
	EXPECT_TRUE(result) <<
		L"Expected result to be non-NULL.";
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected) - 1)) <<
		L"Expected result to be '192.168.101.217'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_Trim1)
{
	const char* ip = "192.168.101.217";
	auto const result = parseIpAddressStringOfLength(ip, strlen(ip) - 1);
	byte expected[] = { 192, 168, 101, 217 };
	EXPECT_TRUE(result) <<
		L"Expected result to be non-NULL.";
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected) - 1)) <<
		L"Expected result to be '192.168.101.217'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_Trim2)
{
	const char* ip = "192.168.101.217";
	auto const result = parseIpAddressStringOfLength(ip, strlen(ip) - 2);
	byte expected[] = { 192, 168, 101, 21 };
	EXPECT_TRUE(result) <<
		L"Expected result to be non-NULL.";
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected) - 1)) <<
		L"Expected result to be '192.168.101.21'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_Trim3)
{
	const char* ip = "192.168.101.217";
	auto const result = parseIpAddressStringOfLength(ip, strlen(ip) - 3);
	EXPECT_FALSE(result) <<
		L"Expected '192.168.101.' to be refused, as it has no fourth part.";
}
TEST(ParseIp, ParseIp_Ipv4_PortNumber)
{
	auto const result = parseIpAddressString("1.2.3.4:80");
	byte expected[] = { 1, 2, 3, 4 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '1.2.3.4'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV4) <<
		L"Expected type to be IPv4";
}
TEST(ParseIp, ParseIp_Ipv4_CIDRFormat)
{
	auto const result = parseIpAddressString("1.2.3.4/32");
	EXPECT_FALSE(result) <<
		L"Expected a prefix length to stop the value being read.";
}
// ------------------------------------------------------------------------------
// IPv6 tests
// ------------------------------------------------------------------------------
TEST(ParseIp, ParseIp_Ipv6_Low)
{
	auto const result = parseIpAddressString("0:0:0:0:0:0:0:0");
	byte expected[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
}
TEST(ParseIp, ParseIp_Ipv6_Low_Abbreviated)
{
	auto const result = parseIpAddressString("::");
	const byte expected[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0'";
}
TEST(ParseIp, ParseIp_Ipv6_High)
{
	auto const result = parseIpAddressString("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF");
	byte expected[] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
}
TEST(ParseIp, ParseIp_Ipv6_AbbreviatedStart)
{
	const char *ipv6AbbreviatedStart = "::FFFF:FFFF:FFFF:FFFF";
	byte expected[IPV6_LENGTH] =
		{ 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255 };

	IpAddress result;
	const bool parsed =
		fiftyoneDegreesIpAddressParse(
			ipv6AbbreviatedStart,
			ipv6AbbreviatedStart + strlen(ipv6AbbreviatedStart),
			&result);

	EXPECT_TRUE(parsed) << "Abbreviated start IPv6 address "
		"should be successfully parsed, where the address is " <<
		ipv6AbbreviatedStart << ".";

	EXPECT_TRUE(result.type == IP_TYPE_IPV6) <<
		"IP address version was not identified correctly where where the " <<
		"IP address is " << ipv6AbbreviatedStart << ".";

	EXPECT_TRUE(
		CheckResult(result.value, expected, IPV6_LENGTH)) <<
		"The value of the abbreivated start IPv6 address is not correctly parsed.";
}
TEST(ParseIp, ParseIp_Ipv6_AbbreviatedMiddle)
{
	auto const result = parseIpAddressString("FFFF:FFFF::FFFF:FFFF");
	byte expected[] = { 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
}
TEST(ParseIp, ParseIp_Ipv6_AbbreviatedEnd)
{
	auto const result = parseIpAddressString("FFFF:FFFF:FFFF:FFFF::");
	byte expected[] = { 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
}
TEST(ParseIp, ParseIp_Ipv6_PortNumber)
{
	auto const result = parseIpAddressString("[2001::1]:80");
	byte expected[] = { 32, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	EXPECT_TRUE(CheckResult(result->value, expected, sizeof(expected))) <<
		L"Expected result to be '32, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1'";
	EXPECT_EQ(result->type, FIFTYONE_DEGREES_IP_TYPE_IPV6) <<
		L"Expected type to be IPv6";
}
TEST(ParseIp, ParseIp_Ipv6_CIDRFormat)
{
	auto const result = parseIpAddressString("2001::1/128");
	EXPECT_FALSE(result) <<
		L"Expected a prefix length to stop the value being read.";
}

// ------------------------------------------------------------------------------
// Invalid data
// ------------------------------------------------------------------------------
TEST(ParseIp, ParseIp_Invalid_ipv4OutOfRange)
{
	auto const result = parseIpAddressString("256.256.256.256");
	EXPECT_FALSE(result) <<
		"Expected an IPv4 address with parts above 255 not to be read.";
}
TEST(ParseIp, ParseIp_Invalid_ipv4TooMany)
{
	auto const result = parseIpAddressString("1.2.3.4.5");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_ipv4TooFew)
{
	auto const result = parseIpAddressString("1.2.3");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_letters)
{
	auto const result = parseIpAddressString("a.b.c.d");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_emptyString)
{
	auto const result = parseIpAddressString("");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_null)
{
	auto const result = parseIpAddressString(nullptr);
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_Number)
{
	auto const result = parseIpAddressString("1234");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_ipv6OutOfRange)
{
	auto const result = parseIpAddressString("10000::1");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_ipv6InvalidLetter)
{
	auto const result = parseIpAddressString("GFFF::1");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_ipv6TooMany)
{
	auto const result = parseIpAddressString("1:1:1:1:1:1:1:1:1");
	EXPECT_FALSE(result);
}
TEST(ParseIp, ParseIp_Invalid_ipv6TooFew)
{
	auto const result = parseIpAddressString("1:1:1:1:1:1:1");
	EXPECT_FALSE(result);
}
// ------------------------------------------------------------------------------
// Whole value rule
//
// A value is read only when all of it is one IPv4 or IPv6 address, with a port
// after an IPv4 address and with brackets around an IPv6 address.
// ------------------------------------------------------------------------------
static void expectRead(
	const char * const text,
	const std::vector<byte> &expected) {
	const IpType expectedType = expected.size() == IPV4_LENGTH ?
		IP_TYPE_IPV4 : IP_TYPE_IPV6;
	auto const result = parseIpAddressString(text);
	ASSERT_TRUE(result) << "Expected '" << text << "' to be read.";
	EXPECT_EQ(result->type, expectedType) << text;
	EXPECT_TRUE(CheckResult(
		result->value,
		expected.data(),
		static_cast<uint16_t>(expected.size()))) <<
		"Wrong address read from '" << text << "'.";
}

static void expectNotRead(const char * const text) {
	EXPECT_FALSE(parseIpAddressString(text)) <<
		"Expected '" << text << "' not to be read.";
}

TEST(ParseIpWhole, ParseIpWhole_Ipv4Forms)
{
	expectRead("1.2.3.4", { 1, 2, 3, 4 });
	expectRead("0.0.0.0", { 0, 0, 0, 0 });
	expectRead("255.255.255.255", { 255, 255, 255, 255 });
	expectRead("10.0.9.100", { 10, 0, 9, 100 });
}
TEST(ParseIpWhole, ParseIpWhole_Ipv4WithPort)
{
	expectRead("1.2.3.4:0", { 1, 2, 3, 4 });
	expectRead("1.2.3.4:80", { 1, 2, 3, 4 });
	expectRead("1.2.3.4:65535", { 1, 2, 3, 4 });
	expectRead("1.2.3.4:00080", { 1, 2, 3, 4 });
}
TEST(ParseIpWhole, ParseIpWhole_Ipv6Forms)
{
	const std::vector<byte> documentation = {
		0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	expectRead("2001:db8::1", documentation);
	expectRead("2001:DB8::1", documentation);
	expectRead("2001:0db8:0000:0000:0000:0000:0000:0001", documentation);
	expectRead("2001:db8:0:0:0:0:0:1", documentation);
	expectRead("::1", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });
	expectRead("::", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
	expectRead("1::", { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
	expectRead("1:2:3:4:5:6:7::",
		{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 0 });
	expectRead("::2:3:4:5:6:7:8",
		{ 0, 0, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8 });
	expectRead("ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", {
		0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
		0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89 });
}
TEST(ParseIpWhole, ParseIpWhole_Ipv6WithIpv4Tail)
{
	const std::vector<byte> mapped = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 129, 144, 52, 38 };
	expectRead("0:0:0:0:0:FFFF:129.144.52.38", mapped);
	expectRead("::FFFF:129.144.52.38", mapped);
	expectRead("::ffff:129.144.52.38", mapped);
	expectRead("0:0:0:0:0:0:13.1.68.3",
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 1, 68, 3 });
	expectRead("::13.1.68.3",
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 1, 68, 3 });
	expectRead("2001:db8::1.2.3.4",
		{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4 });
}
TEST(ParseIpWhole, ParseIpWhole_Ipv6InBrackets)
{
	const std::vector<byte> documentation = {
		0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	expectRead("[2001:db8::1]", documentation);
	expectRead("[2001:db8::1]:80", documentation);
	expectRead("[2001:db8::1]:0", documentation);
	expectRead("[2001:db8::1]:65535", documentation);
	expectRead("[::1]:443", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });
	expectRead("[::ffff:1.2.3.4]:80",
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 2, 3, 4 });
}
// Without brackets the text after the last colon belongs to the address.
TEST(ParseIpWhole, ParseIpWhole_Ipv6WithoutBracketsHasNoPort)
{
	expectRead("2001:db8::1:80",
		{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x80 });
}
TEST(ParseIpWhole, ParseIpWhole_PrefixLengthIsNotRead)
{
	expectNotRead("1.2.3.4/24");
	expectNotRead("1.2.3.4/32");
	expectNotRead("2001:db8::/32");
	expectNotRead("2001:db8::1/128");
	expectNotRead("[2001:db8::1]/64");
}
TEST(ParseIpWhole, ParseIpWhole_TextAroundTheAddressIsNotRead)
{
	expectNotRead("1.2.3.4 x");
	expectNotRead("x 1.2.3.4");
	expectNotRead("1.2.3.4x");
	expectNotRead("1.2.3.4 ");
	expectNotRead(" 1.2.3.4");
	expectNotRead("1.2.3.4\n");
	expectNotRead("1.2.3.4\t");
	expectNotRead("1.2.3.4\r\n");
	expectNotRead("2001:db8::1 ");
	expectNotRead(" 2001:db8::1");
	expectNotRead("[2001:db8::1] ");
	expectNotRead("[2001:db8::1]x");
	expectNotRead("1.2.3.4:80 ");
	expectNotRead("unknown");
	expectNotRead("for=1.2.3.4");
}
TEST(ParseIpWhole, ParseIpWhole_ListOfAddressesIsNotRead)
{
	expectNotRead("1.2.3.4, 5.6.7.8");
	expectNotRead("1.2.3.4,5.6.7.8");
	expectNotRead("1.2.3.4 5.6.7.8");
	expectNotRead("1.2.3.4;5.6.7.8");
	expectNotRead("2001:db8::1, 2001:db8::2");
	expectNotRead("1.2.3.4, 2001:db8::1");
}
TEST(ParseIpWhole, ParseIpWhole_ZoneIndexIsNotRead)
{
	expectNotRead("fe80::1%eth0");
	expectNotRead("fe80::1%1");
	expectNotRead("fe80::1%");
	expectNotRead("[fe80::1%eth0]");
	expectNotRead("[fe80::1%eth0]:80");
	expectNotRead("::ffff:1.2.3.4%1");
}
TEST(ParseIpWhole, ParseIpWhole_BadBracketsAreNotRead)
{
	expectNotRead("[");
	expectNotRead("]");
	expectNotRead("[]");
	expectNotRead("[::1");
	expectNotRead("::1]");
	expectNotRead("::1]:80");
	expectNotRead("[[::1]]");
	expectNotRead("[::1]]");
	expectNotRead("[1.2.3.4]");
	expectNotRead("[1.2.3.4]:80");
	expectNotRead("1.2.3.4]");
	expectNotRead("[1.2.3.4");
}
TEST(ParseIpWhole, ParseIpWhole_BadPortsAreNotRead)
{
	expectNotRead("1.2.3.4:");
	expectNotRead("1.2.3.4:65536");
	expectNotRead("1.2.3.4:99999999999");
	expectNotRead("1.2.3.4:80:90");
	expectNotRead("1.2.3.4:x");
	expectNotRead("1.2.3.4:-1");
	expectNotRead("1.2.3.4:+1");
	expectNotRead("1.2.3.4: 80");
	expectNotRead("[::1]:");
	expectNotRead("[::1]80");
	expectNotRead("[::1]:80x");
	expectNotRead("[::1]:65536");
	expectNotRead("[::1]:80:90");
}
TEST(ParseIpWhole, ParseIpWhole_BadIpv4IsNotRead)
{
	expectNotRead("256.0.0.1");
	expectNotRead("1.2.3.256");
	expectNotRead("256.256.256.256");
	expectNotRead("555.0.0.0");
	expectNotRead("01.2.3.4");
	expectNotRead("1.02.3.4");
	expectNotRead("1.2.3.04");
	expectNotRead("1.2.3");
	expectNotRead("1.2.3.4.5");
	expectNotRead("1.2.3.");
	expectNotRead(".1.2.3");
	expectNotRead("1..2.3");
	expectNotRead("1.2.3.4.");
	expectNotRead("0x1.2.3.4");
	expectNotRead("1.2.3.4a");
	expectNotRead("1.2.3.a");
	expectNotRead("3232235521");
	expectNotRead("1.2.3.-4");
	expectNotRead("1.2.3.+4");
}
TEST(ParseIpWhole, ParseIpWhole_BadIpv6IsNotRead)
{
	expectNotRead("2001:db8::1::2");
	expectNotRead("2001:db8:::1");
	expectNotRead(":::");
	expectNotRead(":1:2:3:4:5:6:7");
	expectNotRead("1:2:3:4:5:6:7:");
	expectNotRead("1:2:3:4:5:6:7");
	expectNotRead("1:2:3:4:5:6:7:8:9");
	expectNotRead("1:2:3:4:5:6:7::8");
	expectNotRead("1:2:3:4:5:6:7:8::");
	expectNotRead("12345::1");
	expectNotRead("g::1");
	expectNotRead("2001:db8::1:");
	expectNotRead("2001:db8:1");
	expectNotRead("2001:db8:1:");
	expectNotRead(":");
	expectNotRead(":1");
	expectNotRead("1:");
}
TEST(ParseIpWhole, ParseIpWhole_BadIpv4TailIsNotRead)
{
	expectNotRead("::1.2.3");
	expectNotRead("::1.2.3.4.5");
	expectNotRead("::1.2.3.4:5");
	expectNotRead("::01.2.3.4");
	expectNotRead("::256.1.1.1");
	expectNotRead("1.2.3.4::");
	expectNotRead("1.2.3.4::1");
	expectNotRead("::1.2.3.4::");
	expectNotRead("1:2:3:4:5:6:1.2.3.4:7");
	expectNotRead("1:2:3:4:5:6:7:1.2.3.4");
	expectNotRead("1:2:3:4:5:1.2.3.4");
	expectNotRead("a.2.3.4:1");
}
TEST(ParseIpWhole, ParseIpWhole_EmbeddedNulIsNotRead)
{
	const char text[] = "1.2.3.4\0x";
	EXPECT_FALSE(parseIpAddressStringOfLength(text, sizeof(text) - 1));
	const char terminated[] = "1.2.3.4";
	EXPECT_FALSE(parseIpAddressStringOfLength(terminated, sizeof(terminated)));
}
TEST(ParseIpWhole, ParseIpWhole_NotReadLeavesTheAddressAlone)
{
	IpAddress address;
	memset(address.value, 0x5a, sizeof(address.value));
	address.type = IP_TYPE_IPV4;
	const char * const text = "1.2.3.4/24";
	EXPECT_FALSE(IpAddressParse(text, text + strlen(text), &address));
	for (size_t i = 0; i < sizeof(address.value); i++) {
		EXPECT_EQ(address.value[i], 0x5a) << "Byte " << i << " changed.";
	}
	EXPECT_EQ(address.type, IP_TYPE_IPV4);
}
// ------------------------------------------------------------------------------
// Comparison
// ------------------------------------------------------------------------------
TEST(CompareIp, CompareIp_Ipv4_Bigger) {
	unsigned char ipAddress1[IPV4_LENGTH] = {9, 8, 7, 6};
	unsigned char ipAddress2[IPV4_LENGTH] = {9, 8, 6, 6};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV4) > 0) << "Result should be positive "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ipv4_Smaller) {
	unsigned char ipAddress1[IPV4_LENGTH] = {3, 4, 5, 6};
	unsigned char ipAddress2[IPV4_LENGTH] = {3, 4, 6, 6};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV4) < 0) << "Result should be negative "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ipv4_Equal) {
	unsigned char ipAddress1[IPV4_LENGTH] = {3, 4, 5, 6};
	unsigned char ipAddress2[IPV4_LENGTH] = {3, 4, 5, 6};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV4) == 0) << "Result should be 0 "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ipv6_Bigger) {
	unsigned char ipAddress1[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
	unsigned char ipAddress2[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 9, 9, 8, 7, 6, 5, 4, 2, 2, 1};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV6) > 0) << "Result should be positive "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ipv6_Smaller) {
	unsigned char ipAddress1[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
	unsigned char ipAddress2[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 12, 9, 9, 8, 7, 6, 5, 4, 4, 2, 1};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV6) < 0) << "Result should be negative "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ipv6_Equal) {
	unsigned char ipAddress1[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
	unsigned char ipAddress2[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_IPV6) == 0) << "Result should be 0 "
		"where IP address 1 is bigger than IP address 2\n";
}

TEST(CompareIp, CompareIp_Ip_Invalid) {
	unsigned char ipAddress1[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
	unsigned char ipAddress2[IPV6_LENGTH] = 
		{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

	EXPECT_TRUE(
		fiftyoneDegreesIpAddressesCompare(
			ipAddress1, 
			ipAddress2, 
			IP_TYPE_INVALID) == 0) << "Result should be 0 "
		"where type is invalid\n";
}
