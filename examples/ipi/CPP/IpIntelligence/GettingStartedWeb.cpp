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

#include <string>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "../../../../src/ipi/EngineIpi.hpp"
#include "ExampleBase.hpp"
#include "../../Base/ExampleBase.h"

using namespace std;
using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;
using namespace FiftyoneDegrees::Examples::IpIntelligence;

/**
@example IpIntelligence/GettingStartedWeb.cpp
Getting started web example of using 51Degrees IP intelligence.

The example is a CGI program. A web server that supports CGI, such as Apache
HTTP Server, IIS or nginx with fcgiwrap, runs it for each request and sends
what it writes to standard output back to the browser. It finds the IP address
of the client from the request, looks it up with the on-premise IP
intelligence engine, and returns a web page showing the properties.

This example is available in full on [GitHub](https://github.com/51Degrees/51degrees-cxx/blob/main/examples/ipi/CPP/IpIntelligence/GettingStartedWeb.cpp).

@include{doc} example-require-datafile-ipi.txt

@include{doc} example-how-to-run-ipi.txt

Run from a console, outside a web server, it writes the page for an example
IP address.

In detail, the example shows how to:

1. Find the IP address of the client. The web server gives the address the
connection came from in the REMOTE_ADDR environment variable. When the request
passed through a proxy or load balancer, that is the proxy's address, and the
client's is the first one in the X-Forwarded-For header, which the web server
gives in HTTP_X_FORWARDED_FOR. Only trust X-Forwarded-For when a proxy you
control sets it, as a client can send any value.
```
string ip = GettingStartedWeb::clientIp(
	getenv("HTTP_X_FORWARDED_FOR"),
	getenv("REMOTE_ADDR"));
```

2. Add the address to the evidence and process it with the engine.
```
EvidenceIpi evidence;
evidence["server.client-ip"] = ip;
unique_ptr<ResultsIpi> results(engine->process(&evidence));
```

3. Write each property to the page, encoding anything that came from the
request, as it is not to be trusted.
```
Value<vector<WeightedValue<string>>> value =
	results->getValuesAsWeightedStringList("RegisteredCountry");
```

The engine is created once for each request here, as a CGI program runs once
for each request. A web server module or an embedded server would create the
engine once when it starts and use it for every request, as the engine can be
used by many threads at once.

*/

namespace FiftyoneDegrees {
	namespace Examples {
		namespace IpIntelligence {
			/**
			 * IP Intelligence Getting Started Web Example.
			 */
			class GettingStartedWeb : public ExampleBase {
			public:
				/**
				 * @copydoc ExampleBase::ExampleBase(string)
				 */
				GettingStartedWeb(string dataFilePath)
					: ExampleBase(dataFilePath) {
				};

				/**
				 * The IP address of the client. That is the first address
				 * in X-Forwarded-For when the header has one, as a proxy
				 * adds the address it received the request from, and
				 * otherwise the address the connection came from.
				 * @param forwardedFor value of the X-Forwarded-For header,
				 * or NULL when the request has none
				 * @param remoteAddr address the connection came from, or
				 * NULL when it is not known
				 * @return the client's IP address, or an empty string when
				 * there is none
				 */
				static string clientIp(
					const char *forwardedFor,
					const char *remoteAddr) {
					if (forwardedFor != nullptr) {
						const string header = forwardedFor;
						size_t start = 0;
						while (start <= header.size()) {
							size_t end = header.find(',', start);
							if (end == string::npos) {
								end = header.size();
							}
							const size_t first =
								header.find_first_not_of(" \t", start);
							if (first != string::npos && first < end) {
								const size_t last =
									header.find_last_not_of(" \t", end - 1);
								return header.substr(first, last - first + 1);
							}
							start = end + 1;
						}
					}
					return remoteAddr == nullptr ? "" : remoteAddr;
				}

				/**
				 * Encodes text so a web page shows it as it is, rather than
				 * reading it as HTML.
				 * @param text to encode
				 * @return the encoded text
				 */
				static string htmlEncode(const string &text) {
					string encoded;
					for (const char c : text) {
						switch (c) {
						case '&': encoded += "&amp;"; break;
						case '<': encoded += "&lt;"; break;
						case '>': encoded += "&gt;"; break;
						case '"': encoded += "&quot;"; break;
						case '\'': encoded += "&#39;"; break;
						default: encoded += c; break;
						}
					}
					return encoded;
				}

				/**
				 * The CGI response for a request: the Content-Type header, a
				 * blank line, and a web page with the client's IP address and
				 * its properties.
				 * @param forwardedFor value of the X-Forwarded-For header,
				 * or NULL when the request has none
				 * @param remoteAddr address the connection came from, or
				 * NULL when it is not known
				 * @return the response
				 */
				string page(
					const char *forwardedFor,
					const char *remoteAddr) const {
					const string ip = clientIp(forwardedFor, remoteAddr);

					// Look up the client's IP address.
					EvidenceIpi evidence;
					evidence["server.client-ip"] = ip;
					const unique_ptr<FiftyoneDegrees::IpIntelligence::ResultsIpi>
						results(
						engine->process(&evidence));

					ostringstream html;
					html << "Content-Type: text/html; charset=utf-8\r\n\r\n";
					html << "<!DOCTYPE html>\n"
						"<html lang=\"en\">\n"
						"<head>\n"
						"<meta charset=\"utf-8\">\n"
						"<title>51Degrees IP Intelligence: Getting Started "
						"Web</title>\n"
						"</head>\n"
						"<body>\n"
						"<h1>51Degrees IP Intelligence</h1>\n";
					html << "<p>Client IP address: <b>" << htmlEncode(ip) <<
						"</b></p>\n";
					html << "<p>X-Forwarded-For: " << htmlEncode(
						forwardedFor == nullptr ? "" : forwardedFor) <<
						"<br>\nConnection from: " << htmlEncode(
						remoteAddr == nullptr ? "" : remoteAddr) << "</p>\n";
					html << "<table>\n"
						"<tr><th>Property</th><th>Value</th>"
						"<th>Weight</th></tr>\n";
					const vector<const char*> props {
						"RegisteredName",
						"RegisteredCountry",
						"IpRangeStart",
						"IpRangeEnd",
						"Latitude",
						"Longitude",
						"Areas",
						"AccuracyRadiusMin",
					};
					for (const char *name : props) {
						if (!hasProperty(name)) {
							html << "<tr><td>" << name << "</td><td>" <<
								htmlEncode(needsEnterprise) <<
								"</td><td></td></tr>\n";
							continue;
						}
						FiftyoneDegrees::Common::Value<
							vector<WeightedValue<string>>> value =
							results->getValuesAsWeightedStringList(name);
						if (value.hasValue() == false) {
							html << "<tr><td>" << name << "</td><td>" <<
								htmlEncode(value.getNoValueMessage()) <<
								"</td><td></td></tr>\n";
							continue;
						}
						for (const WeightedValue<string> &w :
							value.getValue()) {
							html << "<tr><td>" << name << "</td><td>" <<
								htmlEncode(w.getValue()) << "</td><td>" <<
								w.getWeight() << "</td></tr>\n";
						}
					}
					html << "</table>\n";

					// On a free data file, say what the Enterprise data file
					// adds and where to ask for a copy. Only the Enterprise
					// data file has the range properties.
					if (!hasProperty("IpRangeStart")) {
						html << "<p>This example is using a free data file. "
							"The Enterprise data file adds the location, "
							"range and connection properties. Contact us for "
							"a copy at\n<a href=\"" << contactUs << "\">" <<
							contactUs << "</a>\n</p>\n";
					}
					html << "</body>\n</html>\n";
					return html.str();
				}

				/**
				 * @copydoc ExampleBase::run
				 * Run by a web server as a CGI program, the request is read
				 * from the environment the server sets. Run from a console,
				 * the page is for an example IP address.
				 */
				void run() override {
					const char *forwardedFor = nullptr;
					const char *remoteAddr = ipv4Address;
					if (getenv("GATEWAY_INTERFACE") != nullptr) {
						forwardedFor = getenv("HTTP_X_FORWARDED_FOR");
						remoteAddr = getenv("REMOTE_ADDR");
					}
					cout << page(forwardedFor, remoteAddr);
				}

			private:
				/** Where to ask for the Enterprise data file. */
				static constexpr const char *contactUs =
					"https://51degrees.com/contact-us?utm_source=code&utm_medium=example&utm_campaign=51degrees-cxx&utm_content=examples-cpp-ipintelligence-gettingstartedweb.cpp&utm_term=free-data-file";
			};
		}
	}
}

#ifndef TEST

int main(int argc, char *argv[]) {
	fiftyoneDegreesStatusCode status = FIFTYONE_DEGREES_STATUS_SUCCESS;
	char dataFilePath[FIFTYONE_DEGREES_FILE_MAX_PATH];
	// An explicit data file path can be supplied in the _51DEGREES_IPI_PATH
	// environment variable, otherwise the parent folder structure is searched.
	const char *envDataFilePath = ExampleBase::getDataFilePathFromEnvironment();
	if (argc > 1) {
		strcpy(dataFilePath, argv[1]);
	}
	else if (envDataFilePath != NULL && envDataFilePath[0] != '\0') {
		if (strlen(envDataFilePath) >= sizeof(dataFilePath)) {
			status = FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY;
		}
		else {
			strcpy(dataFilePath, envDataFilePath);
		}
	}
	else {
		status = ExampleBase::getFilePath(
				dataDir,
				dataFileName,
				dataFilePath,
				sizeof(dataFilePath));
	}
	if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
		ExampleBase::reportStatus(status, dataFileName);
		return 1;
	}

	try {
		auto const gettingStartedWeb =
			std::make_unique<GettingStartedWeb>(dataFilePath);
		gettingStartedWeb->run();
	}
	catch (const StatusCodeException &e) {
		ExampleBase::reportStatus(e.getCode(), dataFilePath);
		return 1;
	}

	// Standard output is the response, so where to find out more goes to
	// standard error, which the web server writes to its log. A CGI program
	// does not wait for a key press.
	fflush(stdout);
	fiftyoneDegreesExampleFindOutMore(stderr);

	return 0;
}

#endif
