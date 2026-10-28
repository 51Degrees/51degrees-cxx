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

#ifndef FIFTYONE_DEGREES_ENGINE_IP_INTELLIGENCE_TEST_HPP
#define FIFTYONE_DEGREES_ENGINE_IP_INTELLIGENCE_TEST_HPP

#include <string>
#include <cstdlib>
#include "Constants.hpp"
#include "../src/common-cxx/tests/EngineTests.hpp"

// Helper to check if running on CI and config uses temp file (skip to save disk space)
#define SKIP_TEMP_FILE_TESTS_ON_CI() \
	do { \
		const char* ci = std::getenv("CI"); \
		if (ci != nullptr && std::string(ci) == "true" && \
			config != nullptr && config->getConfig().b.useTempFile) { \
			GTEST_SKIP() << "Skipping temp file test on CI (large temp file)"; \
		} \
	} while(0)

// Skip MetaDataReload tests on CI for configs that load all data into memory
// (HighPerformance, InMemory). The MetaDataReload test spawns a reloader thread
// that creates a new full dataset copy every 500ms while worker threads iterate
// metadata. With large enterprise IPI files and in-memory configs, multiple
// concurrent dataset copies accumulate faster than they are released, causing
// the CI runner to run out of memory and cancel the entire job.
// LowMemory and Balanced configs use file-backed access and are not affected.
#define SKIP_IN_MEMORY_RELOAD_TESTS_ON_CI() \
	do { \
		const char* ci = std::getenv("CI"); \
		if (ci != nullptr && std::string(ci) == "true" && config != nullptr) { \
			const auto &ipiConfig = config->getConfig(); \
			if (ipiConfig.b.allInMemory || ipiConfig.strings.loaded) { \
				GTEST_SKIP() << "Skipping MetaDataReload on CI for in-memory " \
					"config (concurrent reloads cause OOM)"; \
			} \
		} \
	} while(0)
#include "../src/EngineIpi.hpp"
#include "../src/common-cxx/textfile.h"

using namespace FiftyoneDegrees::IpIntelligence;

#define ENGINE_PROPERTIES_STRING(n,v) \
static const string n = v; \
static const string *n##Pointer = &n;
#define ENGINE_PROPERTIES_ARRAY_ONE(n,v) \
static const vector<string> n = { v }; \
static const vector<string> *n##Pointer = &n;
#define ENGINE_PROPERTIES_ARRAY_TWO(n,v1,v2) \
static const vector<string> n = { v1, v2 }; \
static const vector<string> *n##Pointer = &n;
#define ENGINE_PROPERTIES_ARRAY_THREE(n,v1,v2,v3) \
static const vector<string> n = { v1, v2, v3 }; \
static const vector<string> *n##Pointer = &n;

// Common IP intelligence properties in arrays and strings.
ENGINE_PROPERTIES_STRING(OnePropertyString, "RegisteredCountry")
ENGINE_PROPERTIES_STRING(TwoPropertyStrings, "RegisteredCountry,RegisteredOwner")
ENGINE_PROPERTIES_STRING(DuplicatePropertyStrings, "RegisteredCountry,RegisteredCountry")
ENGINE_PROPERTIES_STRING(InvalidPropertyStrings, "INVALID,PROPERTIES PROVIDED")
ENGINE_PROPERTIES_STRING(MixedPropertyStrings, "INVALID,RegisteredCountry")
ENGINE_PROPERTIES_STRING(AllEdgePropertyStrings, "RegisteredName,RegisteredOwner,AccuracyRadiusMin")
ENGINE_PROPERTIES_ARRAY_ONE(OnePropertyArray, "RegisteredCountry")
ENGINE_PROPERTIES_ARRAY_TWO(TwoPropertyArray, "RegisteredCountry", "RegisteredOwner")
ENGINE_PROPERTIES_ARRAY_TWO(DuplicatePropertyArray, "RegisteredCountry", "RegisteredCountry")
ENGINE_PROPERTIES_ARRAY_TWO(InvalidPropertyArray, "INVALID1", "INVALID2")
ENGINE_PROPERTIES_ARRAY_TWO(MixedPropertyArray, "RegisteredCountry", "Invalid")
ENGINE_PROPERTIES_ARRAY_THREE(AllEdgePropertyArray, "RegisteredName", "RegisteredOwner", "AccuracyRadiusMin")
static const string *NullPointer = nullptr;

// An ipv4 address string.
static const char* ipv4Address = "8.8.8.8";

// An ipv6 address string.
static const char* ipv6Address = "2001:4860:4860::8888";

// An invalid ipv4 address
static const char* badIpv4Address = "a.b.c.d";

// An lower bound ipv4 address
static const char* lowerBoundIpv4Address = "0.0.0.0";

// An upper bound ipv4 address
static const char* upperBoundIpv4Address = "255.255.255.255";

// An ouput of range ipv4 address
static const char* outOfRangeIpv4Address = "555.0.0.0";

// An invalid ipv6 address
static const char* badIpv6Address = "mmmm:nnnn:mmmm:nnnn:mmmm:nnnn:mmmm:nnnn";

// An lower bound ipv6 address
static const char* lowerBoundIpv6Address = "0000:0000:0000:0000:0000:0000:0000:0000";

// An upper bound ipv6 address
static const char* upperBoundIpv6Address = "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";

// An ouput of range ipv6 address
static const char* outOfRangeIpv6Address = "mmmm:nnnn:mmmm:nnnn:mmmm:nnnn:mmmm:nnnn";

class EngineIpIntelligenceTests : public EngineTests {
public:
	EngineIpIntelligenceTests(
		IpIntelligence::ConfigIpi *config,
		RequiredPropertiesConfig *requiredProperties,
		const char *directory,
		const char **fileNames,
		int fileNamesLength,
		const char *ipAddressesFileName);
	~EngineIpIntelligenceTests() override;
	void SetUp() override;
	void TearDown() override;
	void verifyValueMetaDataIpAddress();
	void metaData();
	void availableProperties();
	string getExpectedFileType();
	void verify() override;
	void randomWithIpAddress(int count);
	void randomWithEvidence(int count);
	void ipAddressPresent(const char *ipAddress);
	void boundIpAddressPresent(const char *ipAddress);
	void randomIpAddressPresent(int count);
	void verifyCityName() {};
	void validateIndex(ResultsBase *results, int index) override;
	void validateName(ResultsBase *results, string *name) override;
	void validateQuick(ResultsBase *results) override;
	void verifyWithEvidence();
	void verifyMixedPrefixesEvidence();
	void verifyTargetIpAddress();
	void verifyWithIpv4Address();
	void verifyWithIpv6Address();
	void verifyWithBadIpv4Address();
	void verifyWithBadIpv6Address();
	void verifyWithBoundIpv4Address();
	void verifyWithBoundIpv6Address();
	void verifyWithOutOfRangeIpv4Address();
	void verifyWithOutOfRangeIpv6Address();
	void verifyWithEmptyIpAddress();
	void verifyWithInvalidInput();
	void verifyWithNullIpAddress();
	void verifyWithNullEvidence();
	void verifyWithEmptyEvidence();
	static void multiThreadRandomRunThread(void* state);
	void multiThreadRandom(uint16_t concurrency);
	void reloadMemory();
	void reloadFile();
	void reloadMemoryFailure();
	void reloadFileFailure();
	EngineBase *getEngine() override;
	EngineIpi *engine;
	IpIntelligence::ConfigIpi *config;
protected:
	vector<string> ipAddresses;
	void compareResults(IpIntelligence::ResultsIpi *a, IpIntelligence::ResultsIpi *b);
	void compareResultsAfterFailedReload(
		IpIntelligence::ResultsIpi *a,
		IpIntelligence::ResultsIpi *b);
	void compareKeys(
		const vector<string> &keysBeforeRefresh,
		const vector<string> &keysAfterRefresh);
	bool fileReadToByteArray();
	void verifyWithIpAddressString(const char *ipAddress);
	void verifyWithEvidence(EvidenceIpi *evidence);
	void verifyTargetIpAddressForString(const char *ipAddress);
	void verifyNoTargetIpAddressForString(const char *ipAddress);
	void verifyTargetIpAddressFromEvidence();
	void expectTargetIpAddress(
		IpIntelligence::ResultsIpi *results,
		const char *expectedIpAddress);
	void verifyComponentMetaDataDefaultProfile(
		MetaData *metaData,
		ComponentMetaData *component) 
		override;
	fiftyoneDegreesMemoryReader data = { nullptr, nullptr, 0L };
private:
	bool validateIpAddressInternal(IpIntelligence::IpAddress ipAddress, int length);
	bool validateIpAddress(IpIntelligence::IpAddress ipAddress);
	void verifyIpAddressValue(const char *ipAddress, Common::Value<IpIntelligence::IpAddress> value);
	static void ipAddressRead(const char *ipAddress, void *state);
	void verifyPropertyValue(
		ResultsBase *results,
		string property,
		string value);
};

// ============================================================================
// TEST MACRO STRATEGY:
// MetaDataReload, Reload, and Size tests depend on CONFIG PROFILE but not on
// PROPERTY COMBINATION. Each config (InMemory, Balanced, LowMemory, etc.) needs
// to be tested, but only with ONE property combination, not all 11.
//
// Current wasteful approach:
//   - 6 configs × 11 properties × 3 slow tests = 198 test runs
// Optimized approach:
//   - 6 configs × 1 property × 3 slow tests = 18 test runs
//
// Use ENGINE_IP_INTELLIGENCE_TESTS for the FIRST property combination of each
// config profile. Use ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD for all other
// property combinations (they still test the config but skip the slow tests).
// ============================================================================

#define ENGINE_IP_INTELLIGENCE_TESTS(e,t,c,p) \
class ENGINE_CLASS_NAME(e,t,c,p) : public ENGINE_CLASS_NAME_BASE(e,t) { \
public: \
	 ENGINE_CLASS_NAME(e,t,c,p)() : ENGINE_CLASS_NAME_BASE(e,t)( \
		new Config##e(ENGINE_CLASS_NAME_CONFIG_POINTER(e,c)), \
		new RequiredPropertiesConfig(p##Pointer), \
		_dataFolderName, \
		_##e##FileNames, \
		_##e##FileNamesLength, \
		_ipAddressesFileName) {} \
	void SetUp() { SKIP_TEMP_FILE_TESTS_ON_CI(); ENGINE_CLASS_NAME_BASE(e,t)::SetUp(); } \
	void TearDown() { ENGINE_CLASS_NAME_BASE(e,t)::TearDown(); } \
}; \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Attributes) { \
	testType(_##e##Product); \
	testPublishedDate(); \
	testUpdateDate(); \
	properties(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Verify) { verify(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), MetaData) { metaData(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), AvailableProperties) { availableProperties(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), MetaDataReload) { metaDataReload(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Reload) { reload(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), ReloadFailure) { reloadFailure(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Size) { size(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Random) { \
	randomWithIpAddress(50); \
	randomWithEvidence(50); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), MultiThreadRandom) { \
	uint16_t c = config->getConcurrency(); \
	multiThreadRandom(c == 0 ? 4 : c); } /* Use 4 threads if no concurrency */

// Variant WITHOUT slow tests (for additional property combinations of the same config)
#define ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e,t,c,p) \
class ENGINE_CLASS_NAME(e,t,c,p) : public ENGINE_CLASS_NAME_BASE(e,t) { \
public: \
	 ENGINE_CLASS_NAME(e,t,c,p)() : ENGINE_CLASS_NAME_BASE(e,t)( \
		new Config##e(ENGINE_CLASS_NAME_CONFIG_POINTER(e,c)), \
		new RequiredPropertiesConfig(p##Pointer), \
		_dataFolderName, \
		_##e##FileNames, \
		_##e##FileNamesLength, \
		_ipAddressesFileName) {} \
	void SetUp() { SKIP_TEMP_FILE_TESTS_ON_CI(); ENGINE_CLASS_NAME_BASE(e,t)::SetUp(); } \
	void TearDown() { ENGINE_CLASS_NAME_BASE(e,t)::TearDown(); } \
}; \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Attributes) { \
	testType(_##e##Product); \
	testPublishedDate(); \
	testUpdateDate(); \
	properties(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Verify) { verify(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), MetaData) { metaData(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), AvailableProperties) { availableProperties(); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), Random) { \
	randomWithIpAddress(50); \
	randomWithEvidence(50); } \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), MultiThreadRandom) { \
	uint16_t c = config->getConcurrency(); \
	multiThreadRandom(c == 0 ? 4 : c); } /* Use 4 threads if no concurrency */

#define ENGINE_IP_INTELLIGENCE_IP_ADDRESS_TESTS(e,t,c,p) \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), TestIpAddress) { \
	ipAddressPresent(ipv4Address); \
	ipAddressPresent(lowerBoundIpv4Address); \
	boundIpAddressPresent(upperBoundIpv4Address); \
	boundIpAddressPresent(outOfRangeIpv4Address); \
	ipAddressPresent(ipv6Address); \
	ipAddressPresent(lowerBoundIpv6Address); \
	boundIpAddressPresent(upperBoundIpv6Address); \
	randomIpAddressPresent(50); \
	verifyMixedPrefixesEvidence(); }
	//boundIpAddressPresent(outOfRangeIpv6Address); \ /* This will give undefined behaviour */

#define ENGINE_IP_INTELLIGENCE_CityName_TESTS(e,t,c,p) \
TEST_F(ENGINE_CLASS_NAME(e,t,c,p), TestCityName) { \
	verifyCityName(); }

#define ENGINE_TEST_CONFIGS(e) \
fiftyoneDegreesConfig##e *ENGINE_CLASS_NAME_CONFIG_POINTER(e, Null) = nullptr; \
ENGINE_CONFIG(e, HighPerformance) \
ENGINE_CONFIG(e, LowMemory) \
ENGINE_CONFIG(e, Balanced) \
ENGINE_CONFIG(e, BalancedTemp) \
ENGINE_CONFIG(e, InMemory)

#define ENGINE_FILE_TESTS(e) \
extern fiftyoneDegreesConfig##e *ENGINE_CLASS_NAME_CONFIG_POINTER(e, Null); \
EXTERN_ENGINE_CONFIG(e, HighPerformance) \
EXTERN_ENGINE_CONFIG(e, LowMemory) \
EXTERN_ENGINE_CONFIG(e, Balanced) \
EXTERN_ENGINE_CONFIG(e, BalancedTemp) \
EXTERN_ENGINE_CONFIG(e, InMemory) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, HighPerformance, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, HighPerformance, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, HighPerformance, Null) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, LowMemory, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, LowMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, LowMemory, Null) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, Balanced, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Balanced, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, Balanced, Null) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, BalancedTemp, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, BalancedTemp, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, InMemory, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, InMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, Null, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, File, Null, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, File, Null, Null) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, InMemory, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, LowMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, Balanced, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, BalancedTemp, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, InMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, Null, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, File, Null, AllEdgePropertyArray)


#define ENGINE_MEMORY_TESTS(e) \
extern fiftyoneDegreesConfig##e *ENGINE_CLASS_NAME_CONFIG_POINTER(e, Null); \
EXTERN_ENGINE_CONFIG(e, HighPerformance) \
EXTERN_ENGINE_CONFIG(e, LowMemory) \
EXTERN_ENGINE_CONFIG(e, Balanced) \
EXTERN_ENGINE_CONFIG(e, BalancedTemp) \
EXTERN_ENGINE_CONFIG(e, InMemory) \
ENGINE_IP_INTELLIGENCE_TESTS(e, Memory, InMemory, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, InMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, Memory, Null, OnePropertyString) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, TwoPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, DuplicatePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, MixedPropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, OnePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, TwoPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, DuplicatePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, MixedPropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS_NO_RELOAD(e, Memory, Null, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_TESTS(e, Memory, InMemory, Null) \
ENGINE_IP_INTELLIGENCE_TESTS(e, Memory, Null, Null) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, Memory, InMemory, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, Memory, InMemory, AllEdgePropertyArray) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, Memory, Null, AllEdgePropertyStrings) \
ENGINE_IP_INTELLIGENCE_CityName_TESTS(e, Memory, Null, AllEdgePropertyArray)

#endif
