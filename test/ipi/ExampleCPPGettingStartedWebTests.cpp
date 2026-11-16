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
 * @file ExampleCPPGettingStartedWebTests.cpp
 * @brief Tests the Getting Started Web example, which the IP Intelligence
 * specification's required examples list. It must find the IP address of the
 * client from the request, taking it from X-Forwarded-For where a proxy set
 * it, process it with the IP Intelligence engine, and show the properties in
 * a web page.
 */

#include "ExampleIpIntelligenceTests.hpp"
#include "../../examples/ipi/CPP/IpIntelligence/GettingStartedWeb.cpp"

// The first address in X-Forwarded-For is the client, and the ones after it
// are the proxies the request passed through.
TEST(ExampleCPPGettingStartedWeb, ClientIp_FirstForwardedFor) {
	EXPECT_EQ("203.0.113.7", GettingStartedWeb::clientIp(
		" 203.0.113.7 , 198.51.100.1", "192.0.2.1"));
	EXPECT_EQ("2001:db8::1", GettingStartedWeb::clientIp(
		"2001:db8::1", "192.0.2.1"));
}

// With no X-Forwarded-For, or one with no address in it, the client is the
// address the connection came from.
TEST(ExampleCPPGettingStartedWeb, ClientIp_RemoteAddr) {
	EXPECT_EQ("192.0.2.1", GettingStartedWeb::clientIp(nullptr, "192.0.2.1"));
	EXPECT_EQ("192.0.2.1", GettingStartedWeb::clientIp("", "192.0.2.1"));
	EXPECT_EQ("192.0.2.1", GettingStartedWeb::clientIp(" , ", "192.0.2.1"));
}

// With neither, there is no address.
TEST(ExampleCPPGettingStartedWeb, ClientIp_None) {
	EXPECT_EQ("", GettingStartedWeb::clientIp(nullptr, nullptr));
}

// The headers come from the client, so the page encodes them before it shows
// them.
TEST(ExampleCPPGettingStartedWeb, HtmlEncode) {
	EXPECT_EQ("&lt;b&gt;&amp;&quot;&#39;",
		GettingStartedWeb::htmlEncode("<b>&\"'"));
	EXPECT_EQ("192.0.2.1", GettingStartedWeb::htmlEncode("192.0.2.1"));
}

class ExampleCPPTestGettingStartedWeb : public ExampleIpIntelligenceTest {
protected:
	/**
	 * True when the test can run. The example opens the data file with the
	 * default configuration, which reads from the file.
	 */
	bool canRun() {
		return fiftyoneDegreesCollectionGetIsMemoryOnly() == false;
	}
};

// The page is a CGI response: a Content-Type header, a blank line and an
// HTML page that names the client's address and each property.
TEST_F(ExampleCPPTestGettingStartedWeb, Page) {
	if (canRun() == false) {
		return;
	}
	GettingStartedWeb example(dataFilePath);
	const std::string page = example.page(
		ExampleBase::ipv4Address, "127.0.0.1");
	EXPECT_EQ(0u, page.find("Content-Type: text/html; charset=utf-8\r\n\r\n"))
		<< page;
	EXPECT_NE(std::string::npos, page.find(
		std::string("Client IP address: <b>") + ExampleBase::ipv4Address +
		"</b>")) << page;
	EXPECT_EQ(std::string::npos, page.find(
		"Client IP address: <b>127.0.0.1</b>")) << page;
	EXPECT_NE(std::string::npos, page.find("RegisteredCountry")) << page;
	EXPECT_NE(std::string::npos, page.find("</html>")) << page;

	// Only a free data file lacks the properties the example marks as needing
	// the Enterprise data file, and only there does the page say where to ask
	// for a copy.
	const bool enterprise =
		dataFilePath.find("Enterprise") != std::string::npos;
	EXPECT_EQ(enterprise, page.find(ExampleBase::needsEnterprise) ==
		std::string::npos) << page;
	EXPECT_EQ(enterprise, page.find("utm_term=free-data-file") ==
		std::string::npos) << page;
}

// A header that is not an address is shown encoded, and the page is still
// made.
TEST_F(ExampleCPPTestGettingStartedWeb, Page_EncodesTheRequest) {
	if (canRun() == false) {
		return;
	}
	GettingStartedWeb example(dataFilePath);
	const std::string page = example.page("<script>", "127.0.0.1");
	EXPECT_EQ(std::string::npos, page.find("<script>")) << page;
	EXPECT_NE(std::string::npos, page.find(
		"Client IP address: <b>&lt;script&gt;</b>")) << page;
	EXPECT_NE(std::string::npos, page.find("</html>")) << page;
}

// Run as a CGI program, the example reads the request from the environment
// the web server sets. Run from a console, with no request, it shows the page
// for an example address.
TEST_F(ExampleCPPTestGettingStartedWeb, Run) {
	if (canRun() == false) {
		return;
	}
	testing::internal::CaptureStdout();
	{
		GettingStartedWeb example(dataFilePath);
		example.run();
	}
	const std::string output = testing::internal::GetCapturedStdout();
	EXPECT_EQ(0u, output.find("Content-Type: text/html")) << output;
	EXPECT_NE(std::string::npos, output.find("</html>")) << output;
}
