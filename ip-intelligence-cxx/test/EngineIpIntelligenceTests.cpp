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

#ifdef _MSC_FULL_VER
#include <string.h>
#else
#include <strings.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#endif
#include <regex>

#include "Constants.hpp"
#include "EngineIpIntelligenceTests.hpp"
#include "../src/EngineIpi.hpp"
#include "../src/common-cxx/file.h"
#include "../src/common-cxx/memory.h"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::IpIntelligence;

EngineIpIntelligenceTests::EngineIpIntelligenceTests(
	ConfigIpi *config,
	RequiredPropertiesConfig *requiredProperties,
	const char *directory,
	const char **fileNames,
	int fileNamesLength,
	const char *ipAddressesFileName)
	: EngineTests(requiredProperties, directory, fileNames, fileNamesLength) {
	char ipAddressesFullName[FIFTYONE_DEGREES_FILE_MAX_PATH];
	char ipAddress[500] = "";
	fiftyoneDegreesFileGetPath(
		directory,
		ipAddressesFileName,
		ipAddressesFullName,
		sizeof(ipAddressesFullName));
	fiftyoneDegreesTextFileIterate(
		ipAddressesFullName, 
		ipAddress, 
		sizeof(ipAddress), 
		this, 
		EngineIpIntelligenceTests::ipAddressRead);
	this->config = config;
	engine = nullptr;
}

EngineIpIntelligenceTests::~EngineIpIntelligenceTests() {
	while (ipAddresses.empty() == false) {
		ipAddresses.pop_back();
	}
}

EngineBase* EngineIpIntelligenceTests::getEngine() { return (EngineBase*)engine; }

void EngineIpIntelligenceTests::SetUp() {
	EngineTests::SetUp();
}

void EngineIpIntelligenceTests::TearDown() {
	if (engine != nullptr) {
		delete engine;
	}
	if (data.current != nullptr) {
		fiftyoneDegreesFree(data.current);
		data.current = nullptr;
		data.length = 0;
	}
	EngineTests::TearDown();
}

void EngineIpIntelligenceTests::verifyComponentMetaDataDefaultProfile(
	MetaData *metaData,
	ComponentMetaData *component) {
	if (component->getDefaultProfileId() != 0) {
		ProfileMetaData *defaultProfile = 
			metaData->getDefaultProfileForComponent(component);
		ComponentMetaData *otherComponent = 
			metaData->getComponentForProfile(defaultProfile);
		ASSERT_EQ(*component, *otherComponent) <<
			L"The component and its default profile are not linked." <<
			L"\nComponent Id = " << (int)component->getComponentId() <<
			L"\nOther Component Id = " << (int)otherComponent->getComponentId() <<
			L"\nProfile Id = " << defaultProfile->getProfileId();

		delete otherComponent;
		delete defaultProfile;
	}
}

/*
 * This test check whether IP address string can be parsed
 * and searched for in values collection where the IP address
 * is in byte format.
 */
void EngineIpIntelligenceTests::verifyValueMetaDataIpAddress() {
	auto const engineIpi = (EngineIpi *)getEngine();
	auto const values = std::unique_ptr<Collection<ValueMetaDataKey, ValueMetaData>>(
		engineIpi->getMetaData()->getValues());
	EXPECT_TRUE(values->getSize() > 0) << "There is no values "
		"meta data.";

	auto const properties = std::unique_ptr<Collection<string, PropertyMetaData>>(
		engineIpi->getMetaData()->getProperties());
	EXPECT_TRUE(properties->getSize() > 0) << "There is no properties "
		"meta data.";

	auto const property = std::unique_ptr<PropertyMetaData>(
		properties->getByKey("IpRangeStart"));
	if (!property) {
		return;
	}
	auto const rangeStartValues = std::unique_ptr<Collection<ValueMetaDataKey, ValueMetaData>>(
		engineIpi->getMetaData()->getValuesForProperty(property.get()));
	EXPECT_TRUE(rangeStartValues->getSize() > 0) << "There is no values "
		"meta data for property " << property->getName();


	// Pick the first value to be used for testing.
	auto const value1 = std::unique_ptr<ValueMetaData>(
		rangeStartValues->getByIndex(1));
	EXPECT_TRUE(value1 != nullptr) << "There is no value at index 0.";
	
	// See if a valid IP address value string can be correctly parsed
	// and searched from the values collection.
	auto const valueName = value1->getName();
	auto const key = ValueMetaDataKey("IpRangeStart", valueName);
	auto const value2 = std::unique_ptr<ValueMetaData>(
		values->getByKey(key));
	EXPECT_TRUE(value2 != nullptr) << "Value meta data is not found "
		"where it should be at IP address: " << value1->getName();
	if (value2) {
		EXPECT_EQ(0, value1->getName().compare(value2->getName()))
			<< "Value meta data is not correct where it should be at IP address: "
			<< value1->getName();
	}
}

void EngineIpIntelligenceTests::metaData() {
	// FIXME: Too long (15+ min)
	// EngineTests::verifyMetaData(getEngine());
	// // Verify additional cases where the value
	// // is not string
	// verifyValueMetaDataIpAddress();
}
void EngineIpIntelligenceTests::availableProperties() {
	// TODO: This mainly check the evidence property which is 
	// not yet applicable to IP intelligence
}

string EngineIpIntelligenceTests::getExpectedFileType() {
	int i;
	for (i = 0; i < _IpiFileNamesLength; i++) {
		if (strcmp(fileName, _IpiFileNames[i]) == 0) {
			return _fileTypes[i];
		}
	}
	return "";
}

void EngineIpIntelligenceTests::ipAddressRead(
	const char *ipAddress,
	void *state) {
	((EngineIpIntelligenceTests*)state)->ipAddresses.push_back(
		string(ipAddress));
}

void EngineIpIntelligenceTests::verifyPropertyValue(
	ResultsBase *results, 
	string property, 
	string value) {
	vector<string> props = results->getProperties();
	if (find(props.begin(), props.end(), property) != props.end()) {
		EXPECT_EQ(*results->getValueAsString(property), value);
	}
}

void EngineIpIntelligenceTests::validateIndex(
	ResultsBase *results,
	int index) {
	ResultsIpi *resultsIpi = (ResultsIpi *)results;
	Common::Value<vector<string>> values = results->getValues(index);
	if (values.hasValue()) {
		EXPECT_NO_THROW(*resultsIpi->getValueAsIpAddress(index)) << "IP address value "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedBoolList(index)) << "Boolean list "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedIntegerList(index)) << "Integer list "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedDoubleList(index)) << "Double list "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedStringList(index)) << "String list "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		EXPECT_NO_THROW(*resultsIpi->getValueAsString(index)) << "String value "
			"for property '" << resultsIpi->getPropertyName(index) << "' at "
			"index '" << index << "' can't throw exception";
		// Scalar bool/integer/double conversions are only defined for a single
		// value. Weighted properties (e.g. CountryCodesGeographical) can resolve
		// to multiple values for some IPs - notably anycast addresses that span
		// several countries - for which these getters throw TooManyValuesException.
		if (values.getValue().size() == 1) {
			EXPECT_NO_THROW(*resultsIpi->getValueAsBool(index)) << "Bool value "
				"for property '" << resultsIpi->getPropertyName(index) << "' at "
				"index '" << index << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValueAsInteger(index)) << "Integer value "
				"for property '" << resultsIpi->getPropertyName(index) << "' at "
				"index '" << index << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValueAsDouble(index)) << "Double value "
				"for property '" << resultsIpi->getPropertyName(index) << "' at "
				"index '" << index << "' can't throw exception";
		}
	}
}

void EngineIpIntelligenceTests::validateName(
	ResultsBase *results,
	string *name) {

	if (isNameAvailable(results, name) == false) {
		// If the name is not available in the properties then when it's 
		// requested an exception should be thrown. 
		EXPECT_THROW(*results->getValues(name),
			InvalidPropertyException) << "Property '" << *name << "' is "
			"missing and should throw exception";
	}
	else {
		ResultsIpi *resultsIpi = (ResultsIpi *)results;
		// If the name is available so the values should be retrieved with
		// out an exception.
		Common::Value<vector<string>> values = results->getValues(name);

		if (values.hasValue() && values.getValue().size() > 0) {
			EXPECT_NO_THROW(*resultsIpi->getValueAsIpAddress(name)) << "IP address value "
				"for property '" << *name << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedBoolList(name)) << "Boolean list "
				"for property '" << *name << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedIntegerList(name)) << "Integer list "
				"for property '" << *name << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedDoubleList(name)) << "Double list "
				"for property '" << *name << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValuesAsWeightedStringList(name)) << "String list "
				"for property '" << *name << "' can't throw exception";
			EXPECT_NO_THROW(*resultsIpi->getValueAsString(name)) << "String value "
				"for property '" << *name << "' can't throw exception";
			// Scalar bool/integer/double conversions are only defined for a
			// single value. Weighted properties (e.g. CountryCodesGeographical)
			// can resolve to multiple values for some IPs - notably anycast
			// addresses spanning several countries - for which these getters
			// throw TooManyValuesException.
			if (values.getValue().size() == 1) {
				EXPECT_NO_THROW(*resultsIpi->getValueAsBool(name)) << "Bool value "
					"for property '" << *name << "' can't throw exception";
				EXPECT_NO_THROW(*resultsIpi->getValueAsInteger(name)) << "Integer value "
					"for property '" << *name << "' can't throw exception";
				EXPECT_NO_THROW(*resultsIpi->getValueAsDouble(name)) << "Double value "
					"for property '" << *name << "' can't throw exception";
			}
		}
		else {
			// There are no values returned. This is only allowed when:
			// 1. If there was no evidence provided. This means the results can
			//    not be determined as there was nothing to process.
			// 2. We don't have enough data for required property.
			EXPECT_TRUE(values.getNoValueReason() ==
				FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULTS ||
				values.getNoValueReason() == 
				FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE) <<
				L"Must get values for available property '" << *name << "'";
		}
	}
}

void EngineIpIntelligenceTests::validateQuick(ResultsBase *results) {
	ResultsIpi* resultsIpi = (ResultsIpi*)results;
	for (int i = 0; i < results->getAvailableProperties(); i++) {
		vector<string> values;
		Common::Value<vector<string>> value = results->getValues(i);
		if (value.hasValue()) {
			EXPECT_NO_THROW(values = *value) << "Should not throw "
			"exception for property '" << results->getPropertyName(i) << "'";
			vector<WeightedValue<string>> weightedStrings = 
				resultsIpi->getValuesAsWeightedStringList(i).getValue();

			float curWeight = 0.0f;
			for (vector<WeightedValue<string>>::iterator iter = weightedStrings.begin();
				iter != weightedStrings.end();
				iter++) {
				if (iter != weightedStrings.begin()) {
					EXPECT_TRUE(curWeight >= iter->getWeight()) << "Weights of returned results "
						"are not in the descending order: " << results->getPropertyName(i);
				}
				curWeight = iter->getWeight();
			}
		}
		else {
			// This could only happen only because we don't have the details
			// in our data.
			EXPECT_THROW(values = *value, NoValuesAvailableException);
		}
	}
}

void EngineIpIntelligenceTests::verifyWithEvidence(EvidenceIpi *evidence) {
	ResultsIpi *results = ((EngineIpi*)getEngine())->process(evidence);
	validate(results);
	delete results;
}

void EngineIpIntelligenceTests::verifyWithEmptyEvidence() {
	EvidenceIpi evidence;
	verifyWithEvidence(&evidence);
}

void EngineIpIntelligenceTests::verifyMixedPrefixesEvidence() {
	EvidenceIpi mixedEvidence;
	// To make sure query.client-ip-51d is pick up first
	mixedEvidence["query.client-ip-51d"] = lowerBoundIpv4Address;
	mixedEvidence["server.true-client-ip-51d"] = upperBoundIpv4Address;
	auto results = std::unique_ptr<ResultsIpi>(
		((EngineIpi*)getEngine())->process(&mixedEvidence));
	if (!results) {
		return;
	}
	Common::Value<IpIntelligence::IpAddress> rangeStart = results->getValueAsIpAddress("IpRangeStart");
	if (!rangeStart.hasValue()) {
		return;
	}
	unsigned char lowerBoundIpAddress[FIFTYONE_DEGREES_IPV4_LENGTH];
	memset(lowerBoundIpAddress, 0, FIFTYONE_DEGREES_IPV4_LENGTH);
	EXPECT_EQ(0,
		memcmp(rangeStart.getValue().getIpAddress(),
			lowerBoundIpAddress,
			FIFTYONE_DEGREES_IPV4_LENGTH)) << "The IpRangeStart IP address is not "
		"at the lower bound where it should be.";

	// Check if true-client-ip-51d is taking priority
	mixedEvidence["query.client-ip-51d"] = "";
	mixedEvidence["query.true-client-ip-51d"] = lowerBoundIpv4Address;
	results = std::unique_ptr<ResultsIpi>(
		((EngineIpi*)getEngine())->process(&mixedEvidence));
	rangeStart = results->getValueAsIpAddress("IpRangeStart");
	EXPECT_EQ(0,
		memcmp(rangeStart.getValue().getIpAddress(),
			lowerBoundIpAddress,
			FIFTYONE_DEGREES_IPV4_LENGTH)) << "The IpRangeStart IP address is not "
		"at the lower bound where it should be.";
}

void EngineIpIntelligenceTests::expectTargetIpAddress(
	ResultsIpi *results,
	const char *expectedIpAddress) {
	const Common::Value<IpIntelligence::IpAddress> target =
		results->getTargetIpAddress();
	ASSERT_TRUE(target.hasValue()) << "The resolved target IP address should "
		"be available whenever the lookup produced results for IP address: "
		<< expectedIpAddress;
	const IpIntelligence::IpAddress expected(expectedIpAddress);
	EXPECT_EQ(expected.getType(), target.getValue().getType()) <<
		"The resolved target IP address is not of the same type as the input "
		"IP address: " << expectedIpAddress;
	const size_t length =
		expected.getType() == FIFTYONE_DEGREES_IP_TYPE_IPV4
			? FIFTYONE_DEGREES_IPV4_LENGTH
			: FIFTYONE_DEGREES_IPV6_LENGTH;
	EXPECT_EQ(0,
		memcmp(
			expected.getIpAddress(),
			target.getValue().getIpAddress(),
			length)) << "The resolved target IP address does not match the "
		"input IP address: " << expectedIpAddress;
}

void EngineIpIntelligenceTests::verifyTargetIpAddressForString(
	const char *ipAddress) {
	auto const engineIpi = (EngineIpi*)getEngine();
	auto const results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(ipAddress));
	// A parseable address always produces one result per available
	// component, so there is always an address to echo back.
	ASSERT_GT(results->results->count, 0u) << "No result was produced for "
		"IP address: " << ipAddress;
	expectTargetIpAddress(results.get(), ipAddress);
}

void EngineIpIntelligenceTests::verifyNoTargetIpAddressForString(
	const char *ipAddress) {
	auto const engineIpi = (EngineIpi*)getEngine();
	auto const results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(ipAddress));
	const char * const reported =
		ipAddress == nullptr ? "(null)" : ipAddress;
	ASSERT_EQ(0u, results->results->count) << "A result was produced for "
		"an input which cannot be parsed: " << reported;
	EXPECT_FALSE(results->getTargetIpAddress().hasValue()) <<
		"A target IP address was returned for an input which cannot be "
		"parsed: " << reported;
}

void EngineIpIntelligenceTests::verifyTargetIpAddressFromEvidence() {
	auto const engineIpi = (EngineIpi*)getEngine();

	// Resolving from evidence needs the data set to declare the header.
	// Assert that rather than skipping quietly, so that the checks below
	// cannot start passing without testing anything.
	const vector<string> * const engineKeys = engineIpi->getKeys();
	ASSERT_NE(
		find(engineKeys->begin(), engineKeys->end(),
			string("query.client-ip-51d")),
		engineKeys->end()) << "The data set does not declare the "
		"client-ip-51d header, so the evidence cannot be resolved.";

	// The query prefix is walked before the server prefix, so the query value
	// is the one the lookup resolves.
	EvidenceIpi mixedEvidence;
	mixedEvidence["query.client-ip-51d"] = ipv4Address;
	mixedEvidence["server.client-ip-51d"] = ipv6Address;
	auto results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(&mixedEvidence));
	ASSERT_GT(results->results->count, 0u) << "No result was produced from "
		"evidence holding a valid client IP address.";
	expectTargetIpAddress(results.get(), ipv4Address);

	// With only the server prefix present that value is resolved instead.
	EvidenceIpi serverEvidence;
	serverEvidence["server.client-ip-51d"] = ipv6Address;
	results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(&serverEvidence));
	ASSERT_GT(results->results->count, 0u) << "No result was produced from "
		"server prefixed evidence holding a valid client IP address.";
	expectTargetIpAddress(results.get(), ipv6Address);

	// Evidence holding no usable IP address resolves nothing.
	EvidenceIpi emptyEvidence;
	results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(&emptyEvidence));
	EXPECT_FALSE(results->getTargetIpAddress().hasValue()) <<
		"A target IP address was returned for empty evidence.";
}

void EngineIpIntelligenceTests::verifyTargetIpAddress() {
	verifyTargetIpAddressForString(ipv4Address);
	verifyTargetIpAddressForString(ipv6Address);
	verifyTargetIpAddressForString(lowerBoundIpv4Address);
	verifyTargetIpAddressForString(upperBoundIpv4Address);
	verifyTargetIpAddressForString(lowerBoundIpv6Address);
	verifyTargetIpAddressForString(upperBoundIpv6Address);
	verifyNoTargetIpAddressForString(badIpv4Address);
	verifyNoTargetIpAddressForString(badIpv6Address);
	verifyNoTargetIpAddressForString("");
	verifyNoTargetIpAddressForString(nullptr);
	verifyTargetIpAddressFromEvidence();
}

void EngineIpIntelligenceTests::verifyWithEvidence() {
	EvidenceIpi queryEvidence, serverEvidence;
	queryEvidence["query.client-ip-51d"] = ipv4Address;
	verifyWithEvidence(&queryEvidence);

	serverEvidence["server.client-ip-51d"] = ipv6Address;
	verifyWithEvidence(&serverEvidence);
}

void EngineIpIntelligenceTests::verifyWithIpAddressString(const char *ipAddress) {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results = engineIpi->process(
		ipAddress);
	validate(results);
	delete results;
}

void EngineIpIntelligenceTests::verifyWithIpv4Address() {
	verifyWithIpAddressString(ipv4Address);
}

void EngineIpIntelligenceTests::verifyWithIpv6Address() {
	verifyWithIpAddressString(ipv6Address);
}

void EngineIpIntelligenceTests::verifyWithBadIpv4Address() {
	verifyWithIpAddressString(badIpv4Address);
}

void EngineIpIntelligenceTests::verifyWithBadIpv6Address() {
	verifyWithIpAddressString(badIpv6Address);
}

void EngineIpIntelligenceTests::verifyWithBoundIpv4Address() {
	verifyWithIpAddressString(lowerBoundIpv4Address);
	verifyWithIpAddressString(upperBoundIpv4Address);
}

void EngineIpIntelligenceTests::verifyWithBoundIpv6Address() {
	verifyWithIpAddressString(lowerBoundIpv6Address);
	verifyWithIpAddressString(upperBoundIpv6Address);
}

void EngineIpIntelligenceTests::verifyWithOutOfRangeIpv4Address() {
	verifyWithIpAddressString(outOfRangeIpv4Address);
}

void EngineIpIntelligenceTests::verifyWithOutOfRangeIpv6Address() {
	verifyWithIpAddressString(outOfRangeIpv6Address);
}


void EngineIpIntelligenceTests::verifyWithInvalidInput() {
	int character;
	char ipAddress[2];
	ipAddress[1] = '\0';
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	for (character = CHAR_MIN; character <= CHAR_MAX; character++) {
		ipAddress[0] = (char)character;
		ResultsIpi *results = engineIpi->process(
		ipAddress);
		validate(results);
		delete results;
	}
}

void EngineIpIntelligenceTests::verifyWithNullEvidence() {
	EngineTests::verifyWithEvidence(nullptr);
}
 
void EngineIpIntelligenceTests::verifyWithNullIpAddress() {
	EngineIpi*engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results = engineIpi->process(
		(const char *)nullptr);
	validate(results);
	delete results;
}
 
void EngineIpIntelligenceTests::verifyWithEmptyIpAddress() {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results = engineIpi->process(
		"");
	validate(results);
	delete results;
}


void EngineIpIntelligenceTests::verify() {
	EngineTests::verify();
	verifyWithEvidence();
	verifyWithIpv4Address();
	verifyWithIpv6Address();
	verifyWithBadIpv4Address();
	verifyWithBadIpv6Address();
	verifyWithBoundIpv4Address();
	verifyWithBoundIpv6Address();
	verifyWithOutOfRangeIpv4Address();
	verifyWithOutOfRangeIpv6Address();
	verifyWithEmptyEvidence();
	verifyWithEmptyIpAddress();
	verifyWithNullIpAddress();
	verifyWithNullEvidence();
	verifyWithInvalidInput();
	verifyTargetIpAddress();
}

bool EngineIpIntelligenceTests::validateIpAddressInternal(
	IpIntelligence::IpAddress ipAddress, int length) {
	const unsigned char *ip = ipAddress.getIpAddress();
	int octet;
	for (int i = 0; i < length; i++) {
		octet = (int)ip[i];
		if (octet < 0 || octet > 255) {
			return false;
		}
	}
	return true;
}

bool EngineIpIntelligenceTests::validateIpAddress(
	IpIntelligence::IpAddress ipAddress) {
	bool result = false;

	switch(ipAddress.getType()) {
	case FIFTYONE_DEGREES_IP_TYPE_IPV4:
		result = validateIpAddressInternal(ipAddress, FIFTYONE_DEGREES_IPV4_LENGTH);
		break;
	case FIFTYONE_DEGREES_IP_TYPE_IPV6:
		result = validateIpAddressInternal(ipAddress, FIFTYONE_DEGREES_IPV6_LENGTH);
		break;
	default:
		break;
	}
	return result;
}

void EngineIpIntelligenceTests::verifyIpAddressValue(
	const char *ipAddress, 
	Common::Value<IpIntelligence::IpAddress> value) {
	if (value.hasValue()) {
		EXPECT_EQ(true, validateIpAddress(value.getValue())) << "An invalid IP address has been "
			"returned, where it should be for IP address: " << ipAddress;
	}
}

void EngineIpIntelligenceTests::ipAddressPresent(const char *ipAddress) {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results = engineIpi->process(ipAddress);
	Common::Value<IpIntelligence::IpAddress> rangeStart = results->getValueAsIpAddress("IpRangeStart");
	Common::Value<IpIntelligence::IpAddress> rangeEnd = results->getValueAsIpAddress("IpRangeEnd");

	verifyIpAddressValue(ipAddress, rangeStart);
	verifyIpAddressValue(ipAddress, rangeEnd);

	delete results;
}

void EngineIpIntelligenceTests::boundIpAddressPresent(const char *ipAddress) {
	unsigned char lowerBoundIpAddress[FIFTYONE_DEGREES_IPV6_LENGTH];
	unsigned char upperBoundIpAddress[FIFTYONE_DEGREES_IPV6_LENGTH];
	memset(lowerBoundIpAddress, 0, FIFTYONE_DEGREES_IPV6_LENGTH);
	memset(upperBoundIpAddress, 0xff, FIFTYONE_DEGREES_IPV6_LENGTH);

	auto const engineIpi = (EngineIpi*)getEngine();
	auto const results = std::unique_ptr<ResultsIpi>(
		engineIpi->process(ipAddress));
	Common::Value<IpIntelligence::IpAddress> rangeStart = results->getValueAsIpAddress("IpRangeStart");
	Common::Value<IpIntelligence::IpAddress> rangeEnd = results->getValueAsIpAddress("IpRangeEnd");

	if (!rangeStart.hasValue()) {
		return;
	}

	verifyIpAddressValue(ipAddress, rangeStart);
	verifyIpAddressValue(ipAddress, rangeEnd);

	EXPECT_EQ(rangeStart.getValue().getType(), rangeEnd.getValue().getType())
			<< "IpRangeStart and IpRangeEnd types are not the same, where it should "
			"be at IP address: " << ipAddress;

	if (rangeStart.getValue().getType() == FIFTYONE_DEGREES_IP_TYPE_IPV4) {
		EXPECT_TRUE(
			memcmp(
				lowerBoundIpAddress,
				rangeStart.getValue().getIpAddress(),
				FIFTYONE_DEGREES_IPV4_LENGTH) == 0 ||
			memcmp(
				upperBoundIpAddress,
				rangeEnd.getValue().getIpAddress(),
				FIFTYONE_DEGREES_IPV4_LENGTH) == 0) << "IpRangeStart or IpRangeEnd are not "
			"at the bound where it should be at IP address: " << ipAddress;
	}
	else {
		EXPECT_TRUE(
			memcmp(
				lowerBoundIpAddress,
				rangeStart.getValue().getIpAddress(),
				FIFTYONE_DEGREES_IPV6_LENGTH) == 0 ||
			memcmp(
				upperBoundIpAddress,
				rangeEnd.getValue().getIpAddress(),
				FIFTYONE_DEGREES_IPV6_LENGTH) == 0) << "IpRangeStart or IpRangeEnd are not "
			"at the bound where it should be at IP address: " << ipAddress;
	}
}

void EngineIpIntelligenceTests::randomIpAddressPresent(int count) {
	auto const engineIpi = (EngineIpi*)getEngine();

	for (int i = 0; i < count; i++) {
		string ipAddress = ipAddresses[rand() % ipAddresses.size()];
		auto const results = std::unique_ptr<ResultsIpi>(
			engineIpi->process(ipAddress.c_str()));

		Common::Value<IpIntelligence::IpAddress> rangeStart = results->getValueAsIpAddress("IpRangeStart");
		Common::Value<IpIntelligence::IpAddress> rangeEnd = results->getValueAsIpAddress("IpRangeEnd");

		if (!rangeStart.hasValue()) {
			continue;
		}

		verifyIpAddressValue(ipAddress.c_str(), rangeStart);
		verifyIpAddressValue(ipAddress.c_str(), rangeEnd);

		EXPECT_EQ(rangeStart.getValue().getType(), rangeEnd.getValue().getType())
			<< "IpRangeStart and IpRangeEnd types are not the same, where it should "
			"be at IP address: " << ipAddress;
		EXPECT_TRUE(fiftyoneDegreesIpAddressesCompare(
			rangeStart.getValue().getIpAddress(),
			rangeEnd.getValue().getIpAddress(),
			rangeStart.getValue().getType()) < 0) << "Range start IP address should "
			"be smaller than Range end IP address, where it shoud for IP address: " <<
			ipAddress;
	}
}

void EngineIpIntelligenceTests::randomWithIpAddress(int count) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(count);
#	endif
	// FIXME: Unstable on CI
	// EngineIpi *engineIpi = (EngineIpi*)getEngine();
	// for (int i = 0; i < count; i++) {
	// 	string ipAddress = ipAddresses[rand() % ipAddresses.size()];
	// 	ResultsIpi *results = engineIpi->process(
	// 		ipAddress.c_str());
	// 	validateQuick(results);
	// 	delete results;
	// }
}

void EngineIpIntelligenceTests::randomWithEvidence(int count) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(count);
#	endif
	// FIXME: Unstable on CI
	// string ipKey = "query.client-ip-51d";
	// EngineIpi *engineIpi = (EngineIpi*)getEngine();
	// for (int i = 0; i < count; i++) {
	// 	EvidenceIpi evidence;
	// 	evidence[ipKey] = ipAddresses[rand() % ipAddresses.size()].c_str();
	// 	ResultsIpi *results = engineIpi->process(&evidence);
	// 	validateQuick(results);
	// 	delete results;
	// }
}

void EngineIpIntelligenceTests::multiThreadRandomRunThread(void* state) {
	((EngineIpIntelligenceTests*)state)->randomWithIpAddress(200);
	((EngineIpIntelligenceTests*)state)->randomWithEvidence(200);
	FIFTYONE_DEGREES_THREAD_EXIT;
}

/**
 * Check that multiple threads can fetch items from the cache safely.
 * NOTE: it is important that 'number of threads' <=
 * 'number of values' / 'number of threads'. This prevents null from being
 * returned by the cache.
 * @param concurrency number of threads to run the test with
 */
void EngineIpIntelligenceTests::multiThreadRandom(uint16_t concurrency) {
	if (fiftyoneDegreesThreadingGetIsThreadSafe() == false) {
		return;
	}
	ASSERT_NE(nullptr, getEngine());
	runThreads(
		concurrency,
		(FIFTYONE_DEGREES_THREAD_ROUTINE)multiThreadRandomRunThread);
}

 void EngineIpIntelligenceTests::compareResults(
 	ResultsIpi *a, 
 	ResultsIpi *b) {
 	EXPECT_NE(a->results->b.dataSet, b->results->b.dataSet) <<
 		"The data set was not reloaded.";
 	EXPECT_EQ(a->getAvailableProperties(), b->getAvailableProperties()) <<
 		"Number of properties available does not match.";
 	for (size_t i = 0; i < a->getProperties().size(); i++) {
		Common::Value<vector<string>> av = a->getValues((int)i);
		Common::Value<vector<string>> bv = b->getValues((int)i);
		if (av.hasValue()) {
			EXPECT_TRUE(bv.hasValue()) << "Expected both has values.";
			vector<string> avs = *a->getValues((int)i);
 			vector<string> bvs = *b->getValues((int)i);
 			EXPECT_EQ(avs.size(), bvs.size()) << "Expected same number of values.";
 			for (size_t v = 0; v < avs.size(); v++) {
 				EXPECT_STREQ(avs[v].c_str(), bvs[v].c_str()) <<
 					"Values for the new data set should be the same.";
 			}
		}
		else {
			// One does not have values, the other should not have value either.
			EXPECT_FALSE(bv.hasValue()) << "Expected both dot not have values.";
		}
 	}
 }
 
void EngineIpIntelligenceTests::compareKeys(
	const vector<string> &keysBeforeRefresh,
	const vector<string> &keysAfterRefresh) {
	EXPECT_FALSE(keysAfterRefresh.empty()) <<
		"The evidence keys were not rebuilt after the data set was "
		"reloaded.";
	EXPECT_EQ(keysBeforeRefresh, keysAfterRefresh) <<
		"Reloading the same data file should leave the evidence keys "
		"unchanged.";
}

bool EngineIpIntelligenceTests::fileReadToByteArray() {
	fiftyoneDegreesStatusCode status = fiftyoneDegreesFileReadToByteArray(
		fullName,
		&data);
	return status == FIFTYONE_DEGREES_STATUS_SUCCESS;
}
 
void EngineIpIntelligenceTests::reloadFile() {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results1 = engineIpi->process(
		ipv4Address);
	const vector<string> keysBeforeRefresh = *engineIpi->getKeys();
	engineIpi->refreshData();
	compareKeys(keysBeforeRefresh, *engineIpi->getKeys());
	ResultsIpi *results2 = engineIpi->process(
		ipv4Address);
	compareResults(results1, results2);
	delete results1;
	delete results2;
}
 
void EngineIpIntelligenceTests::reloadMemory() {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results1 = engineIpi->process(
		ipv4Address);
	fiftyoneDegreesMemoryReader newData;
	fiftyoneDegreesStatusCode status = fiftyoneDegreesFileReadToByteArray(
		fullName,
		&newData);
	EXPECT_EQ(status, FIFTYONE_DEGREES_STATUS_SUCCESS) << "New data could "
		"not be loaded into memory from '" << fullName << "'";
	EXPECT_NE(newData.current, nullptr) << "New data could "
		"not be loaded into memory from '" << fullName << "'";
	const vector<string> keysBeforeRefresh = *engineIpi->getKeys();
	engineIpi->refreshData(newData.current, newData.length);
	compareKeys(keysBeforeRefresh, *engineIpi->getKeys());
	ResultsIpi *results2 = engineIpi->process(
		ipv4Address);
	compareResults(results1, results2);
	delete results1;
	delete results2;

	// Now that the results1 has been deleted free the memory used by the 
	// now replaced original dataset. Set the data that will be freed 
	// during tear down to that which is now active in memory.
	fiftyoneDegreesFree(data.current);
	data = newData;
}

/*
 * Fills the buffer with a data set header carrying version 1.0 so any
 * attempt to initialise a data set from it must fail with the
 * INCORRECT_VERSION status after the replacement data set has been
 * allocated and, for a file reload, its file handles opened.
 */
static void fillWithInvalidVersion(unsigned char *buffer, size_t length) {
	int32_t versionMajor = 1;
	int32_t versionMinor = 0;
	memset(buffer, 0, length);
	memcpy(buffer, &versionMajor, sizeof(int32_t));
	memcpy(buffer + sizeof(int32_t), &versionMinor, sizeof(int32_t));
}

/*
 * Checks that after a failed reload the engine still answers queries
 * with the data set that was active before the reload was attempted.
 */
void EngineIpIntelligenceTests::compareResultsAfterFailedReload(
	ResultsIpi *a,
	ResultsIpi *b) {
	EXPECT_EQ(a->results->b.dataSet, b->results->b.dataSet) <<
		"The failed reload did not leave the original data set active.";
	EXPECT_EQ(a->getAvailableProperties(), b->getAvailableProperties()) <<
		"Number of properties available does not match.";
}

void EngineIpIntelligenceTests::reloadFileFailure() {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results1 = engineIpi->process(
		ipv4Address);

	// Write a file which must fail to load to a temporary path.
	unsigned char invalidData[sizeof(fiftyoneDegreesDataSetIpiHeader) + 64];
	fillWithInvalidVersion(invalidData, sizeof(invalidData));
	const char *invalidFileName = "invalid-version-reload.tmp";
	fiftyoneDegreesStatusCode status = fiftyoneDegreesFileWrite(
		invalidFileName,
		invalidData,
		sizeof(invalidData));
	ASSERT_EQ(status, FIFTYONE_DEGREES_STATUS_SUCCESS) << "The invalid "
		"data file could not be written to '" << invalidFileName << "'";

	// The reload must report the failure to the caller, and must leave
	// nothing allocated behind it. The engine test suites restore the
	// standard allocators in SetUp, so the suite's own memory check never
	// sees anything. Tracking is turned on around the failed reload alone,
	// where everything allocated is also freed within the window, so the
	// leak is caught on every platform rather than only where the C runtime
	// provides its own check.
	fiftyoneDegreesSetUpMemoryTracking();
	EXPECT_THROW(
		engineIpi->refreshData(invalidFileName),
		StatusCodeException) << "Reloading from an invalid data file "
		"must fail.";
	EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking()) << "The "
		"failed reload from a file did not free everything it allocated.";

	ResultsIpi *results2 = engineIpi->process(
		ipv4Address);
	compareResultsAfterFailedReload(results1, results2);
	delete results1;
	delete results2;

	// Deleting the file also checks the failed reload closed the file
	// handles it opened, as an open handle stops the delete on Windows.
	// The memory check in TearDown asserts the replacement data set the
	// failed reload allocated was freed.
	EXPECT_EQ(
		fiftyoneDegreesFileDelete(invalidFileName),
		FIFTYONE_DEGREES_STATUS_SUCCESS) << "The invalid data file could "
		"not be deleted, so the failed reload may still hold a handle "
		"to it.";
}

void EngineIpIntelligenceTests::reloadMemoryFailure() {
	EngineIpi *engineIpi = (EngineIpi*)getEngine();
	ResultsIpi *results1 = engineIpi->process(
		ipv4Address);

	// A buffer which must fail to load as a data set.
	unsigned char invalidData[sizeof(fiftyoneDegreesDataSetIpiHeader) + 64];
	fillWithInvalidVersion(invalidData, sizeof(invalidData));

	// The reload must report the failure to the caller, and must leave
	// nothing allocated behind it, the copy the engine takes of the buffer
	// included. See reloadFileFailure for why the check brackets the
	// reload rather than relying on the suite's own memory check.
	fiftyoneDegreesSetUpMemoryTracking();
	EXPECT_THROW(
		engineIpi->refreshData(
			invalidData,
			(fiftyoneDegreesFileOffset)sizeof(invalidData)),
		StatusCodeException) << "Reloading from an invalid data buffer "
		"must fail.";
	EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking()) << "The "
		"failed reload from memory did not free everything it allocated.";

	ResultsIpi *results2 = engineIpi->process(
		ipv4Address);
	compareResultsAfterFailedReload(results1, results2);
	delete results1;
	delete results2;
}

ENGINE_TEST_CONFIGS(Ipi)
