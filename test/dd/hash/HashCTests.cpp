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

#include "../../common/pch.h"
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../common/SizeOtherThread.hpp"
#include "../../../src/dd/hash/PropertyMetaDataBuilderHash.hpp"
#include "../../../src/dd/hash/ValueMetaDataBuilderHash.hpp"
#include "../../../src/dd/hash/fiftyone.h"


// User-Agent string of an iPhone mobile device.
const char* mobileUserAgent = (
	"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
	"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
	"Safari/9537.53");

const char* commonProperties =
	"ScreenPixelsWidth,HardwareModel,IsMobile,BrowserName";

using namespace std;

/** Results that are freed on every way out of a test. */
typedef std::unique_ptr<
	ResultsHash,
	decltype(&ResultsHashFree)> OwnedResults;

/**
 * Frees an initialised manager on every way out of a test. Declare it before
 * any results made from the manager, so that the results are freed first.
 */
typedef std::unique_ptr<
	ResourceManager,
	decltype(&ResourceManagerFree)> ManagerGuard;

class HashCTests : public Base {
public:
	HashCTests() {
		dataFilePath = "";
		for (int i = 0;
			i < _HashFileNamesLength && strcmp("", dataFilePath.c_str()) == 0;
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
	}

	void SetUp() {
		Base::SetUp();
		internalSetUp();
	}
	void TearDown() {
		internalTearDown();
		Base::TearDown();
	}
protected:
	/*
	* Actual SetUp for this test.
	* All SetUp tasks should be done here. This is to allow test that do not
	* require common resource to be able to control these resource via call
	* to internalSetUp and internalTearDown.
	*/
	void internalSetUp() {
		properties.string = commonProperties;
		configHash.traceRoute = true;
		
		EXCEPTION_CREATE;
		// Init manager
		HashInitManagerFromFile(
			&manager,
			&configHash,
			&properties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
	}
	
	/*
	* Actual TearDown for this test.
	* All teardown tasks should be donw here. This is to allow test that do not
	* require common resource to be able to control these resource via call
	* to internalSetUp and internalTearDown.
	*/
	void internalTearDown() {
		ResourceManagerFree(&manager);
	}

	string dataFilePath;
	PropertiesRequired properties = PropertiesDefault;
	ConfigHash configHash = HashDefaultConfig;
	ResourceManager manager;
};

static int getRequiredPropertyIndex(
	ResultsHash* results,
	const char* propertyName) {
	DataSetHash* dataSet = (DataSetHash*)results->b.b.dataSet;
	return PropertiesGetRequiredPropertyIndexFromName(
		dataSet->b.b.available,
		propertyName);
}

static char* getPropertyValueAsString(
	ResultsHash* results,
	const char* propertyName,
	char *buffer,
	size_t bufferSize) {
	EXCEPTION_CREATE;
	buffer[0] = '\0';
	ResultsHashGetValuesString(
		results,
		propertyName,
		buffer,
		bufferSize,
		(char* const)",",
		exception);
	EXCEPTION_THROW;
	return buffer;
}

/*
 * Check that the API ResultsHashFromDeviceId returned
 * correct result
 */
TEST_F (HashCTests, ResultsHashFromDeviceIdTest) {
	ResultsHash* resultsUserAgents;
	ResultsHash* resultsDeviceId;

	string testPropertyName = "IsMobile";
	char deviceId[40] = "";
	char isMobile[40] = "";

	resultsUserAgents = ResultsHashCreate(&manager, 0);
	resultsDeviceId = ResultsHashCreate(&manager, 0);

	EXCEPTION_CREATE
	// Obtain results from user agent
	ResultsHashFromUserAgent(
		resultsUserAgents,
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW;

	// A single User-Agent now yields the unified evidence-driven shape: one
	// result item per available component, matching ResultsHashFromEvidence.
	EXPECT_EQ(
		((DataSetHash*)resultsUserAgents->b.b.dataSet)->componentsAvailableCount,
		resultsUserAgents->count) << "One result per available component "
		<< "should be returned.\n";
	EXPECT_EQ(true, ResultsHashGetHasValues(
		resultsUserAgents,
		getRequiredPropertyIndex(resultsUserAgents, testPropertyName.c_str()),
		exception));
	EXCEPTION_THROW;
	EXPECT_EQ(0, strcmp(
		"True",
		getPropertyValueAsString(
			resultsUserAgents,
			testPropertyName.c_str(),
			isMobile,
			sizeof(isMobile)))) << "Property " + testPropertyName + " should be true.\n";

	// Obtain device ID from result
	HashGetDeviceIdFromResults(
		resultsUserAgents,
		(char*)deviceId,
		sizeof(deviceId),
		exception);
	EXCEPTION_THROW;

	// Obtain result again from device ID
	ResultsHashFromDeviceId(
		resultsDeviceId,
		deviceId,
		sizeof(deviceId),
		exception);
	EXCEPTION_THROW;

	memset(isMobile, 0, sizeof(isMobile));
	EXPECT_EQ(1, resultsDeviceId->count) << "Only one results should be "
		<< "returned from detection using device ID.\n";
	EXPECT_EQ(true, ResultsHashGetHasValues(
		resultsDeviceId,
		getRequiredPropertyIndex(resultsDeviceId, testPropertyName.c_str()),
		exception));
	EXCEPTION_THROW;
	EXPECT_EQ(0, strcmp(
		"True",
		getPropertyValueAsString(
			resultsDeviceId,
			testPropertyName.c_str(),
			isMobile,
			sizeof(isMobile)))) << "Property " + testPropertyName + " should be true.\n";

	// Free the results and resource
	ResultsHashFree(resultsUserAgents);
	ResultsHashFree(resultsDeviceId);
}

/*
 * Regression for issue #362. The result shape must be a function of the data
 * set, not of evidence cardinality. A single 'header.user-agent' pair (Case A)
 * and the same User-Agent with a redundant, lower-precedence 'query.user-agent'
 * pair (Case B) must produce identical result counts and identify the same
 * device. The public ResultsHashFromUserAgent entry point (Case C) must agree
 * with them. Before the fast path was removed, Case A returned a single
 * coalesced item while Case B returned one item per component.
 */
TEST_F(HashCTests, ResultsShapeIndependentOfEvidenceCardinality) {
	EXCEPTION_CREATE;

	// Case A: a single User-Agent supplied as an HTTP header.
	EvidenceKeyValuePairArray* evidenceA = EvidenceCreate(1);
	EvidenceAddString(
		evidenceA,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"User-Agent",
		mobileUserAgent);
	ResultsHash* resultsA = ResultsHashCreate(&manager, 0);
	ResultsHashFromEvidence(resultsA, evidenceA, exception);
	EXCEPTION_THROW;

	// Case B: the same User-Agent plus a redundant query User-Agent. Query has
	// precedence over header but resolves to the same string, so detection is
	// identical; only the number of evidence pairs differs from Case A.
	EvidenceKeyValuePairArray* evidenceB = EvidenceCreate(2);
	EvidenceAddString(
		evidenceB,
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		"User-Agent",
		mobileUserAgent);
	EvidenceAddString(
		evidenceB,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"User-Agent",
		mobileUserAgent);
	ResultsHash* resultsB = ResultsHashCreate(&manager, 0);
	ResultsHashFromEvidence(resultsB, evidenceB, exception);
	EXCEPTION_THROW;

	// Case C: the public convenience entry point for a bare User-Agent.
	ResultsHash* resultsC = ResultsHashCreate(&manager, 0);
	ResultsHashFromUserAgent(
		resultsC, mobileUserAgent, strlen(mobileUserAgent), exception);
	EXCEPTION_THROW;

	// The regression invariant for #362: all three shapes must agree,
	// whatever the data file provides.
	EXPECT_EQ(resultsA->count, resultsB->count) <<
		"Redundant evidence must not change the result count.\n";
	EXPECT_EQ(resultsA->count, resultsC->count) <<
		"FromUserAgent must produce the same shape as FromEvidence.\n";
	// Secondary: with a User-Agent every available component resolves, so
	// the unified shape is one result item per available component.
	DataSetHash* dataSet = (DataSetHash*)resultsA->b.b.dataSet;
	EXPECT_EQ(dataSet->componentsAvailableCount, resultsA->count) <<
		"One result per available component should be returned.\n";

	char idA[80] = "", idB[80] = "", idC[80] = "";
	HashGetDeviceIdFromResults(resultsA, idA, sizeof(idA), exception);
	HashGetDeviceIdFromResults(resultsB, idB, sizeof(idB), exception);
	HashGetDeviceIdFromResults(resultsC, idC, sizeof(idC), exception);
	EXCEPTION_THROW;
	EXPECT_STREQ(idA, idB) <<
		"Redundant evidence must not change the detected device.\n";
	EXPECT_STREQ(idA, idC) <<
		"FromUserAgent must detect the same device as FromEvidence.\n";

	ResultsHashFree(resultsA);
	ResultsHashFree(resultsB);
	ResultsHashFree(resultsC);
	EvidenceFree(evidenceA);
	EvidenceFree(evidenceB);
}

/*
 * Regression for issue #362. When the property's component has no matched
 * profile in any result item and unmatched results are not allowed
 * (allowUnmatched == false, the default), GetNoValueReason must report
 * NULL_PROFILE - whose message points the caller at lenient matching - not the
 * generic NO_RESULT_FOR_PROPERTY.
 *
 * Removing the single-User-Agent fast path changed the result shape from one
 * coalesced item to one item per component. That routed value selection down
 * the multi-result path, which returns no result at all for a component with
 * only null profiles, so GetNoValueReason fell through to
 * NO_RESULT_FOR_PROPERTY instead of the informative NULL_PROFILE it returned
 * before. This is user-visible in every wrapper (.NET/Java/Node/Python/PHP).
 *
 * A component with no matched profile arises naturally on data files whose
 * components a single User-Agent does not all populate (e.g. a crawler
 * component), but the Lite file used here resolves every component for a real
 * User-Agent via the predictive graph. To reproduce the null-profile state
 * deterministically we clear the profile offsets on the result items - exactly
 * the state a genuinely unpopulated component leaves behind - and assert the
 * reason. Before the fix this returns NO_RESULT_FOR_PROPERTY.
 */
TEST_F(HashCTests, NoValueReasonForNullProfileComponentIsNullProfile) {
	EXCEPTION_CREATE;

	std::unique_ptr<EvidenceKeyValuePairArray, decltype(&EvidenceFree)>
		evidence(EvidenceCreate(1), EvidenceFree);
	EvidenceAddString(
		evidence.get(),
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"User-Agent",
		mobileUserAgent);

	OwnedResults results(ResultsHashCreate(&manager, 0), ResultsHashFree);
	DataSetHash* dataSet = (DataSetHash*)results->b.b.dataSet;
	ASSERT_FALSE(dataSet->config.b.allowUnmatched) <<
		"This regression only applies when unmatched results are disabled.\n";

	ResultsHashFromEvidence(results.get(), evidence.get(), exception);
	EXCEPTION_THROW;
	ASSERT_GT(results->count, 0u) <<
		"A User-Agent should produce at least one result.\n";

	// Walk every graph, so that no read walks one after the profiles are set
	// below.
	ResultsEvaluateAll(&results->b.b, exception);
	EXCEPTION_THROW;

	// Force the null-profile state: no result item carries a profile for any
	// component, as happens for a component the evidence does not resolve. The
	// engine marks an absent profile with UINT32_MAX (its internal
	// NULL_PROFILE_OFFSET sentinel, which is not exported).
	const uint32_t nullProfileOffset = UINT32_MAX;
	for (uint32_t i = 0; i < results->count; i++) {
		for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
			results->items[i].profileOffsets[c] = nullProfileOffset;
			results->items[i].profileIsOverriden[c] = false;
		}
	}

	const int requiredPropertyIndex =
		getRequiredPropertyIndex(results.get(), "IsMobile");
	ASSERT_GE(requiredPropertyIndex, 0) <<
		"IsMobile must be an available property for this test.\n";

	ASSERT_FALSE(ResultsHashGetHasValues(
		results.get(), requiredPropertyIndex, exception)) <<
		"A component with only null profiles must have no value.\n";
	EXCEPTION_THROW;

	fiftyoneDegreesResultsNoValueReason reason = ResultsHashGetNoValueReason(
		results.get(), requiredPropertyIndex, exception);
	EXCEPTION_THROW;
	EXPECT_EQ(FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE, reason) <<
		"A component with only null profiles should report a null profile, "
		"pointing the caller at lenient matching, not a generic "
		"no-result-for-property reason.\n";
}

/*
 * Check that the API ResultsHashGetValuesString correctly
 * deal with invalid uniqueHttpHeaderIndex for a single result.
 */
TEST_F(HashCTests, ResultsHashGetValuesStringTest) {
	ResultsHash* resultsUserAgents;
	ResultsHash* resultsDeviceId;

	char deviceId[40] = "";
	char isMobile[40] = "";

	resultsUserAgents = ResultsHashCreate(&manager, 0);
	resultsDeviceId = ResultsHashCreate(&manager, 0);

	// Obtain result again from device ID
	// with invalid uniqueHttpHeaderIndex
	resultsDeviceId->items[0].b.uniqueHttpHeaderIndex = -2;
	memset(isMobile, 0, sizeof(isMobile));
	EXCEPTION_CREATE;
	size_t charsAdded = ResultsHashGetValuesString(
		resultsDeviceId,
		"isMobile",
		isMobile,
		sizeof(isMobile),
		(char* const)",",
		exception);
	EXCEPTION_THROW;
    
    //StringBuilder adds 1 char which is a null-terminator
	EXPECT_EQ(1, charsAdded) << "No result should have been found where "
		<< "uniqueHttpHeaderIndex is "
		<< resultsDeviceId->items[0].b.uniqueHttpHeaderIndex
		<< "\n";

	// Obtain result again from device ID
	// with invalid uniqueHttpHeaderIndex
	DataSetHash* dataSet = (DataSetHash*)resultsDeviceId->b.b.dataSet;
	resultsDeviceId->items[0].b.uniqueHttpHeaderIndex =
		dataSet->b.b.uniqueHeaders->count + 1;
	memset(isMobile, 0, sizeof(isMobile));
	charsAdded = ResultsHashGetValuesString(
		resultsDeviceId,
		"isMobile",
		isMobile,
		sizeof(isMobile),
		(char* const)",",
		exception);
	EXCEPTION_THROW;
    //StringBuilder adds 1 char which is a null-terminator
	EXPECT_EQ(1, charsAdded) << "No result should have been found where "
		<< "uniqueHttpHeaderIndex is "
		<< resultsDeviceId->items[0].b.uniqueHttpHeaderIndex
		<< "\n";

	// Free the results and resource
	ResultsHashFree(resultsUserAgents);
	ResultsHashFree(resultsDeviceId);
}

/*
 * Test if the graph trace get API deal with buffer correctly. Check
 * potentially written number of characters are returned even if the buffer
 * does not have enough space.
 */
TEST_F(HashCTests, GraphTraceGetTests) {
	OwnedResults resultsUserAgents(
		ResultsHashCreate(&manager, 0),
		ResultsHashFree);

	EXCEPTION_CREATE;
	// Obtain results from user agent, walking every graph so that the traces
	// are written.
	ResultsHashFromUserAgent(
		resultsUserAgents.get(),
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW;
	ResultsEvaluateAll(&resultsUserAgents->b.b, exception);
	EXCEPTION_THROW;

	// Test if GraphTraceGet returns potentially written number
	char buffer[1] = "";
	// Test with 0
	int potentiallyWritten = GraphTraceGet(
		buffer,
		0,
		resultsUserAgents->items[0].trace,
		mobileUserAgent);
	EXPECT_TRUE(potentiallyWritten > 0) <<
		"Potentially written number should have been returned.\n";

	// Test with 1
	potentiallyWritten = GraphTraceGet(
		buffer,
		1,
		resultsUserAgents->items[0].trace,
		mobileUserAgent);
	EXPECT_TRUE(potentiallyWritten > 0) <<
		"Potentially written number should have been returned.\n";

	// Test if GraphTraceGet returns correct written number
	// Add 1 for null character
	int length = potentiallyWritten + 1;
	char* fullBuffer = (char*)Malloc(length);
	EXPECT_TRUE(fullBuffer != NULL) <<
		"Failed to allocate memory for graph trace.\n";

	memset(fullBuffer, 0, length);
	int written = GraphTraceGet(
		fullBuffer,
		length,
		resultsUserAgents->items[0].trace,
		mobileUserAgent);
	EXPECT_EQ(strlen(fullBuffer), written) <<
		"Failed to write the full graph trace.\n";
	EXPECT_EQ(potentiallyWritten, written) <<
		"Failed to return corrent number of written characters.\n";

	// Free resource
	Free(fullBuffer);
}

// TODO - Remove after refactor.
/*
 * Check that the creation of ResultsHash create evidence array correctly
 * with and without pseudo headers.
 */
//TEST_F(HashCTests, ResultsHashCreation) {
//	ResultsHash *testResults1, *testResults2;
//
//	DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
//	uint32_t savePseudoHeaderCount =
//		dataSet->b.b.uniqueHeaders->pseudoHeadersCount;
//
//	// Create addtional results and pseudo evidence
//	// if Client Hints are enabled and pseudo headers
//	// are present.
//	dataSet->b.b.uniqueHeaders->pseudoHeadersCount = 2;
//	testResults1 = ResultsHashCreate(&manager, 0);
//	EXPECT_TRUE(testResults1->pseudoEvidence != NULL);
//	EXPECT_EQ(2, testResults1->pseudoEvidence->capacity);
//	EXPECT_EQ(3, testResults1->capacity);
//
//	// Don't create addtional results and pseudo evidence
//	// if pseudo headers are not present.
//	dataSet->b.b.uniqueHeaders->pseudoHeadersCount = 0;
//	testResults2 = ResultsHashCreate(&manager, 0);
//	EXPECT_TRUE(testResults2->pseudoEvidence == NULL);
//	EXPECT_EQ(1, testResults2->capacity);
//
//	dataSet->b.b.uniqueHeaders->pseudoHeadersCount = savePseudoHeaderCount;
//	DataSetRelease((DataSetBase *)dataSet);
//	// Free allocated resource
//	ResultsHashFree(testResults1);
//	ResultsHashFree(testResults2);
//}

// TODO - Remove after refactor.
/*
 * This test check that the detection will still work when there is no
 * pseudo header count
 */
//TEST_F(HashCTests, ResultsHashFromEvidencePseudoEvidenceCreation) {
//	ResultsHash* resultsUserAgents;
//
//	char isMobile[40] = "";
//
//	DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
//	uint32_t savePseudoHeaderCount =
//		dataSet->b.b.uniqueHeaders->pseudoHeadersCount;
//
//	// Set the pseudo header count to mock scenarios
//	// where data file does not support pseudo headers
//	dataSet->b.b.uniqueHeaders->pseudoHeadersCount = 0;
//	resultsUserAgents = ResultsHashCreate(&manager, 0);
//	EXPECT_TRUE(resultsUserAgents->pseudoEvidence == NULL);
//
//	fiftyoneDegreesEvidenceKeyValuePairArray* evidence =
//		EvidenceCreate(1);
//	const char* evidenceField = "User-Agent";
//	const char* evidenceValue = mobileUserAgent;
//	EvidenceAddString(
//		evidence,
//		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
//		evidenceField,
//		evidenceValue);
//
//	// Obtain results from user agent
//	EXCEPTION_CREATE
//	ResultsHashFromEvidence(resultsUserAgents, evidence, exception);
//	EXCEPTION_THROW;
//	EXPECT_EQ(1, resultsUserAgents->count) << "Only one results should be "
//		<< "returned.\n";
//	EXPECT_EQ(0, strcmp(
//		"True",
//		getPropertyValueAsString(
//			resultsUserAgents,
//			"isMobile",
//			isMobile,
//			sizeof(isMobile)))) << "Property isMobile should be true.\n";
//
//	dataSet->b.b.uniqueHeaders->pseudoHeadersCount = savePseudoHeaderCount;
//	EvidenceFree(evidence);
//	DataSetRelease((DataSetBase*)dataSet);
//	// Free allocated resource
//	ResultsHashFree(resultsUserAgents);
//}

/*
 * This test check that the ResultsHashGetValuesString will only add separator
 * if there is next value.
 */
TEST_F(HashCTests, ResultsHashGetValuesStringNoTrailingSeparator) {
	ResultsHash* results = ResultsHashCreate(&manager, 0);

	EXCEPTION_CREATE;
	// Obtain results from user agent
	ResultsHashFromUserAgent(
		results,
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW;

	char buffer[100] = "";
	ResultsHashGetValuesString(
		results, "IsMobile", buffer, 100, (char* const)",", exception);
	ResultsHashFree(results);

	EXPECT_STREQ("True", buffer) <<
		"Buffer should only contain a string without separator.\n";
}

/*
 * This test check that the HashSizeManagerFromFile will set the exception
 * status correctly if an error occurred during the sizing.
 */
TEST_F(HashCTests, HashSizeManagerFromFileException) {
	// fiftyoneDegreesHashSizeManagerFromFile use memory tracking to size the
	// memory space that will be allocated. This would cause the memory
	// check for this test to fail due to tracked memory allocated in SetUp
	// being lost due to a Reset in HashSizeManagerFromFile happens before those
	// memory is freed. Thus, free them here.
	internalTearDown();

	EXCEPTION_CREATE;
	fiftyoneDegreesHashSizeManagerFromFile(
		&this->configHash,
		&this->properties,
		"donotexist",
		exception);
	EXPECT_TRUE(EXCEPTION_CHECK(FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND)) <<
		"Exception status should be set to " <<
		FIFTYONE_DEGREES_STATUS_FILE_NOT_FOUND << ".\n";

	// SetUp the test at this point so the test will perform memory free as
	// normal and memory check will not fail for this test.
	internalSetUp();
}

/*
 * Check that a null configuration, which the comments on the method allow,
 * means the balanced one for a data file that loads.
 */
TEST_F(HashCTests, HashSizeManagerFromFileNullConfig) {
	internalTearDown();

	EXCEPTION_CREATE;
	const size_t size = fiftyoneDegreesHashSizeManagerFromFile(
		nullptr,
		&this->properties,
		dataFilePath.c_str(),
		exception);
	EXPECT_TRUE(EXCEPTION_OKAY);
	const size_t balanced = fiftyoneDegreesHashSizeManagerFromFile(
		&fiftyoneDegreesHashBalancedConfig,
		&this->properties,
		dataFilePath.c_str(),
		exception);
	EXPECT_TRUE(EXCEPTION_OKAY);
	EXPECT_GT(size, (size_t)0);
	EXPECT_EQ(balanced, size);

	internalSetUp();
}

/*
 * Check that the size from a file counts only the memory of the thread that
 * measures it, and keeps no record of memory another thread allocates.
 */
TEST_F(HashCTests, HashSizeManagerFromFileOtherThread) {
	internalTearDown();
	expectSizeIgnoresOtherThread([&]() {
		EXCEPTION_CREATE;
		const size_t size = fiftyoneDegreesHashSizeManagerFromFile(
			&fiftyoneDegreesHashInMemoryConfig,
			&this->properties,
			dataFilePath.c_str(),
			exception);
		EXPECT_TRUE(EXCEPTION_OKAY);
		return size;
	});
	internalSetUp();
}

/*
 * Check that the size from memory counts only the memory of the thread that
 * measures it, and keeps no record of memory another thread allocates.
 */
TEST_F(HashCTests, HashSizeManagerFromMemoryOtherThread) {
	internalTearDown();
	std::ifstream file(dataFilePath, std::ios::binary);
	ASSERT_TRUE(file.good()) << "Could not open '" << dataFilePath << "'.";
	std::vector<char> data(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	ASSERT_FALSE(data.empty());
	expectSizeIgnoresOtherThread([&]() {
		EXCEPTION_CREATE;
		const size_t size = fiftyoneDegreesHashSizeManagerFromMemory(
			&fiftyoneDegreesHashInMemoryConfig,
			&this->properties,
			data.data(),
			(long)data.size(),
			exception);
		EXPECT_TRUE(EXCEPTION_OKAY);
		return size;
	});
	std::vector<char>().swap(data);
	internalSetUp();
}

TEST_F(HashCTests, BalancedPresetsHoldThePropertiesInMemory) {
	const ConfigHash* presets[] = {
		&HashBalancedConfig,
		&HashDefaultConfig,
		&HashBalancedTempConfig };
	for (const ConfigHash* preset : presets) {
		EXPECT_TRUE(preset->properties.loaded);
	}
	// The manager is made with the Default preset.
	DataSetHash* dataSet = DataSetHashGet(&manager);
	EXPECT_STREQ("CollectionMemory", dataSet->properties->typeName);
	DataSetHashRelease(dataSet);
}

/**
 * Check that when an index for a property was not found (i.e. the index is -1)
 * an approriate error is set, and there is no segfault.
 */
TEST_F(HashCTests, HashResultsGetValuesNoPropertyIndex) {
	ResultsHash* results = ResultsHashCreate(&manager, 0);

	EXCEPTION_CREATE;
	// Obtain results from user agent
	ResultsHashFromUserAgent(
		results,
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW;
	Item item;
	DataReset(&item.data);
	ResultsHashGetValues(results, -1, exception);
	ResultsHashFree(results);

	EXPECT_FALSE(EXCEPTION_OKAY);
	EXPECT_TRUE(EXCEPTION_CHECK(COLLECTION_INDEX_OUT_OF_RANGE));
}

/**
 * Check that when an index for a property is out of range,
 * an approriate error is set, and there is no segfault.
 */
TEST_F(HashCTests, HashResultsGetValuesOutOfRangePropertyIndex) {
	ResultsHash* results = ResultsHashCreate(&manager, 0);
	DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);

	EXCEPTION_CREATE;
	// Obtain results from user agent
	ResultsHashFromUserAgent(
		results,
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW;
	Item item;
	DataReset(&item.data);
	// One past the last required property index, which counts the names
	// given, so a data file without one of them still makes it out of range.
	ResultsHashGetValues(
		results,
		(int)fiftyoneDegreesPropertiesGetRequiredPropertyCount(
			dataSet->b.b.available),
		exception);
	ResultsHashFree(results);
	DataSetHashRelease(dataSet);

	EXPECT_FALSE(EXCEPTION_OKAY);
	EXPECT_TRUE(EXCEPTION_CHECK(COLLECTION_INDEX_OUT_OF_RANGE));
}

static void* getFail(
	const fiftyoneDegreesCollection * const collection,
	const CollectionKey * const key,
	fiftyoneDegreesCollectionItem * const item,
	fiftyoneDegreesException * const exception) {
#	ifdef _MSC_VER
	UNREFERENCED_PARAMETER(collection);
	UNREFERENCED_PARAMETER(key);
	UNREFERENCED_PARAMETER(item);
#	endif
	EXCEPTION_SET(CORRUPT_DATA);
	return nullptr;
}

/**
 * Check that when an exception is set by the values collection,
 * an approriate error is set, and there is no segfault.
 */
TEST_F(HashCTests, HashResultsGetValuesNoProfileValues) {
	ResultsHash* results = ResultsHashCreate(&manager, 0);

	EXCEPTION_CREATE;
	DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
	fiftyoneDegreesCollectionGetMethod oldGetValue = dataSet->values->get;
	dataSet->values->get = getFail;
	// Obtain results from user agent
	ResultsHashFromUserAgent(
		results,
		mobileUserAgent,
		strlen(mobileUserAgent),
		exception);
	EXCEPTION_THROW; 
	Item item;
	DataReset(&item.data);
	ResultsHashGetValues(results, -1, exception);
	dataSet->values->get = oldGetValue;

	ResultsHashFree(results);
	DataSetHashRelease(dataSet);

	EXPECT_FALSE(EXCEPTION_OKAY);
	EXPECT_TRUE(EXCEPTION_CHECK(COLLECTION_INDEX_OUT_OF_RANGE));
}

/*
 * Tests for the way the list of override values is sized and filled, for
 * issue #411. Which properties evidence can override differs between data
 * files, so each test takes the names it needs from the data set rather than
 * naming them, and says so where a data file cannot carry the case. Every
 * property is required so that the properties evidence can override, and the
 * JavaScript properties which measure them, are all available.
 */
#ifndef FIFTYONE_DEGREES_REDUCED_FILE
/** Values whose metadata the deprecated builder taking a value builds. */
static const uint32_t deprecatedBuilderValues = 1000;

/*
 * Reads the description and URL of every value in the data file, through the
 * C functions and through the meta data the language wrappers use, and the URL
 * of every property. The
 * configuration decides whether the strings come from memory, a cache or the
 * file, and so how the item holding each one is released. A value with no
 * description or URL points at offset 0, which holds the first string in the
 * file.
 */
static void checkValueDescriptions(const char *fileName, ConfigHash config) {
	using FiftyoneDegrees::DeviceDetection::Hash::ValueMetaDataBuilderHash;
	EXCEPTION_CREATE;
	ResourceManager manager;
	PropertiesRequired properties = PropertiesDefault;
	properties.string = commonProperties;
	StatusCode status = HashInitManagerFromFile(
		&manager,
		&config,
		&properties,
		fileName,
		exception);
	ASSERT_EQ(SUCCESS, status);
	ASSERT_TRUE(EXCEPTION_OKAY);
	DataSetHash *dataSet = DataSetHashGet(&manager);
	for (uint32_t i = 0; i < dataSet->header.values.count; i++) {
		Item valueItem, stringItem;
		DataReset(&valueItem.data);
		DataReset(&stringItem.data);
		const Value *value = ValueGet(
			dataSet->values, i, &valueItem, exception);
		ASSERT_NE(nullptr, value);
		const String *description = fiftyoneDegreesValueGetDescription(
			dataSet->strings, value, &stringItem, exception);
		ASSERT_TRUE(EXCEPTION_OKAY);
		ASSERT_NE(nullptr, description);
		auto metaData = ValueMetaDataBuilderHash::build(dataSet, i);
		ASSERT_NE(nullptr, metaData);
		EXPECT_STREQ(&description->value, metaData->getDescription().c_str());
		if (value->descriptionOffset <= 0) {
			ASSERT_STREQ("", &description->value) <<
				"The value at index " << i << " has no description.";
		}
		Item urlItem;
		DataReset(&urlItem.data);
		const String *url = fiftyoneDegreesValueGetUrl(
			dataSet->strings, value, &urlItem, exception);
		ASSERT_TRUE(EXCEPTION_OKAY);
		ASSERT_NE(nullptr, url);
		EXPECT_STREQ(&url->value, metaData->getUrl().c_str());
		// The deprecated builder taking the value gives the same, checked on
		// the first values only, as every value is read twice otherwise.
		if (i < deprecatedBuilderValues) {
			auto fromValue = ValueMetaDataBuilderHash::build(dataSet, value);
			ASSERT_NE(nullptr, fromValue);
			EXPECT_EQ(metaData->getKey().getPropertyName(),
				fromValue->getKey().getPropertyName());
			EXPECT_EQ(metaData->getName(), fromValue->getName());
			EXPECT_EQ(metaData->getDescription(),
				fromValue->getDescription());
			EXPECT_EQ(metaData->getUrl(), fromValue->getUrl());
			delete fromValue;
		}
		if (value->urlOffsetOrWeight == 0) {
			ASSERT_STREQ("", &url->value) <<
				"The value at index " << i << " has no URL.";
		}
		delete metaData;
		COLLECTION_RELEASE(dataSet->strings, &urlItem);
		COLLECTION_RELEASE(dataSet->strings, &stringItem);
		COLLECTION_RELEASE(dataSet->values, &valueItem);
	}
	for (uint32_t i = 0; i < dataSet->header.properties.count; i++) {
		Item propertyItem, urlItem;
		DataReset(&propertyItem.data);
		DataReset(&urlItem.data);
		const Property *property = PropertyGet(
			dataSet->properties, i, &propertyItem, exception);
		ASSERT_NE(nullptr, property);
		const String *url = fiftyoneDegreesPropertyGetUrl(
			dataSet->strings, property, &urlItem, exception);
		ASSERT_TRUE(EXCEPTION_OKAY);
		ASSERT_NE(nullptr, url);
		if (property->urlOffset == 0) {
			ASSERT_STREQ("", &url->value) <<
				"The property at index " << i << " has no URL.";
		}
		COLLECTION_RELEASE(dataSet->strings, &urlItem);
		COLLECTION_RELEASE(dataSet->properties, &propertyItem);
	}
	DataSetHashRelease(dataSet);
	ResourceManagerFree(&manager);
}

TEST_F(HashCTests, ValueDescriptions) {
	checkValueDescriptions(dataFilePath.c_str(), HashInMemoryConfig);
#ifndef FIFTYONE_DEGREES_MEMORY_ONLY
	checkValueDescriptions(dataFilePath.c_str(), HashLowMemoryConfig);
	checkValueDescriptions(dataFilePath.c_str(), HashBalancedConfig);
#endif
}
#endif

/**
 * The type name the property meta data gives for each value type a Hash data
 * file holds, or nullptr for a type no Hash data file holds.
 */
static const char* hashTypeName(const Property *property) {
	switch (property->valueType) {
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING:
		return property->isList != 0 ? "string[]" : "string";
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_INTEGER:
		return "int";
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DOUBLE:
		return "double";
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_BOOLEAN:
		return "bool";
	case FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_JAVASCRIPT:
		return "javascript";
	default:
		return nullptr;
	}
}

class HashPropertyTypeTests : public Base {};

/**
 * The type name of every property of each Hash data file found, through the
 * meta data the language wrappers read. A property of a type hashTypeName
 * does not name fails, because each language maps these names to types of its
 * own, and a type new to Hash data files needs a mapping there first.
 */
TEST_F(HashPropertyTypeTests, EveryDataFile) {
	using FiftyoneDegrees::DeviceDetection::Hash::PropertyMetaDataBuilderHash;
	int files = 0;
	for (int f = 0; f < _HashFileNamesLength; f++) {
		const string path = GetFilePath(_dataFolderName, _HashFileNames[f]);
		if (path.empty()) {
			continue;
		}
		files++;
		EXCEPTION_CREATE;
		ResourceManager manager;
		PropertiesRequired properties = PropertiesDefault;
		ConfigHash config = HashDefaultConfig;
		ASSERT_EQ(SUCCESS, HashInitManagerFromFile(
			&manager,
			&config,
			&properties,
			path.c_str(),
			exception)) << path;
		DataSetHash *dataSet = DataSetHashGet(&manager);
		for (uint32_t i = 0; i < dataSet->header.properties.count; i++) {
			Item item;
			DataReset(&item.data);
			const Property *property = PropertyGet(
				dataSet->properties, i, &item, exception);
			ASSERT_NE(nullptr, property) << path << " property " << i;
			std::unique_ptr<FiftyoneDegrees::Common::PropertyMetaData> metaData(
				PropertyMetaDataBuilderHash::build(dataSet, i));
			const char *expected = hashTypeName(property);
			if (expected == nullptr) {
				ADD_FAILURE() << _HashFileNames[f] << " " <<
					metaData->getName() << " has value type " <<
					(int)property->valueType << ", which is named " <<
					metaData->getType();
			}
			else {
				EXPECT_EQ(expected, metaData->getType()) <<
					_HashFileNames[f] << " " << metaData->getName();
			}
			COLLECTION_RELEASE(dataSet->properties, &item);
		}
		DataSetHashRelease(dataSet);
		ResourceManagerFree(&manager);
	}
	if (files == 0) {
		GTEST_SKIP() << "No Hash data file was found.";
	}
}

class HashCOverridesTests : public Base {
public:
	HashCOverridesTests() {
		dataFilePath = "";
		for (int i = 0;
			i < _HashFileNamesLength && strcmp("", dataFilePath.c_str()) == 0;
			i++) {
			dataFilePath = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
	}

	void SetUp() {
		Base::SetUp();
		EXCEPTION_CREATE;
		properties.string = NULL;
		HashInitManagerFromFile(
			&manager,
			&configHash,
			&properties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
	}

	void TearDown() {
		ResourceManagerFree(&manager);
		Base::TearDown();
	}

protected:
	/*
	 * The names of the properties that evidence can override.
	 */
	vector<string> getOverridableNames() {
		vector<string> names;
		DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
		if (dataSet->b.b.overridable != NULL) {
			for (uint32_t i = 0; i < dataSet->b.b.overridable->count; i++) {
				names.push_back(string(STRING(
					dataSet->b.b.overridable->items[i]
						.available->name.data.ptr)));
			}
		}
		DataSetHashRelease(dataSet);
		return names;
	}

	/*
	 * The names of the properties that evidence can override and that the
	 * engine does not empty as the JavaScript property measuring another
	 * property, which it does for a property whose name starts with the name
	 * of another. A test sending a value for one of those would find the
	 * value replaced by the empty one, which is the engine working rather
	 * than a fault.
	 */
	vector<string> getOverridableNamesNotEmptied() {
		vector<string> names;
		DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
		if (dataSet->b.b.overridable != NULL) {
			for (uint32_t i = 0; i < dataSet->b.b.overridable->count; i++) {
				string name = string(STRING(
					dataSet->b.b.overridable->items[i]
						.available->name.data.ptr));
				bool measuresAnother = false;
				for (uint32_t j = 0;
					j < dataSet->b.b.available->count &&
						measuresAnother == false;
					j++) {
					string other = string(STRING(
						dataSet->b.b.available->items[j].name.data.ptr));
					measuresAnother =
						other.length() < name.length() &&
						name.compare(0, other.length(), other) == 0;
				}
				if (measuresAnother == false) {
					names.push_back(name);
				}
			}
		}
		DataSetHashRelease(dataSet);
		return names;
	}

	string dataFilePath;
	PropertiesRequired properties = PropertiesDefault;
	ConfigHash configHash = HashDefaultConfig;
	ResourceManager manager;
};

/*
 * The list of override values is sized by the data set rather than by the
 * number the caller passes, so a caller asking for overrides gets room for
 * every property the data set lets evidence override however little evidence
 * the request carries. A caller asking for more than that still gets what it
 * asked for, and a caller asking for none still gets none. See #411.
 */
TEST_F(HashCOverridesTests, OverrideListIsSizedFromTheDataSet) {
	EXCEPTION_CREATE;
	uint32_t overridableCount = (uint32_t)getOverridableNames().size();
	if (overridableCount < 2) {
		// The Visual Studio test project builds against a googletest that
		// has no skip, so the test reports why it checked nothing and
		// returns.
		SUCCEED() << "The data file offers fewer than two properties that "
			"evidence can override, so there is nothing to check.\n";
		return;
	}

	// One item of evidence, fewer than the properties the data set lets
	// evidence override.
	EvidenceKeyValuePairArray* evidence = EvidenceCreate(1);
	EvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"User-Agent",
		mobileUserAgent);

	ResultsHash* results = ResultsHashCreate(&manager, 1);
	ResultsHashFromEvidence(results, evidence, exception);
	EXCEPTION_THROW;
	EXPECT_EQ(overridableCount, results->b.b.overrides->capacity) <<
		"The list should hold one item for each property the data set lets "
		"evidence override, whatever the request carries.\n";

	// A caller asking for more than the data set needs gets what it asked
	// for.
	ResultsHash* larger = ResultsHashCreate(&manager, overridableCount + 10);
	EXPECT_EQ(overridableCount + 10, larger->b.b.overrides->capacity) <<
		"A caller asking for a longer list should still get one.\n";

	// A caller asking for no overrides still gets none, which is how
	// overrides are turned off.
	ResultsHash* none = ResultsHashCreate(&manager, 0);
	EXPECT_EQ(0, none->b.b.overrides->capacity) <<
		"A capacity of zero should still turn overrides off.\n";

	ResultsHashFree(results);
	ResultsHashFree(larger);
	ResultsHashFree(none);
	EvidenceFree(evidence);
}

/*
 * That size is only enough because every value the engine stores is a value
 * for a property the data set lets evidence override. The values taken from
 * the evidence are, by definition. The empty values are for JavaScript
 * properties, so this checks that every JavaScript property in the data set
 * is one evidence can override. If a data file ever breaks that, this test
 * fails rather than a browser being sent a snippet it has already run. See
 * #411.
 */
TEST_F(HashCOverridesTests, EveryJavaScriptPropertyCanBeOverridden) {
	EXCEPTION_CREATE;
	DataSetHash* dataSet = (DataSetHash*)DataSetGet(&manager);
	int javaScriptProperties = 0;
	for (uint32_t i = 0; i < dataSet->b.b.available->count; i++) {
		if (fiftyoneDegreesPropertyGetValueType(
			dataSet->properties,
			dataSet->b.b.available->items[i].propertyIndex,
			exception) == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_JAVASCRIPT) {
			javaScriptProperties++;
			bool overridable = false;
			if (dataSet->b.b.overridable != NULL) {
				for (uint32_t j = 0;
					j < dataSet->b.b.overridable->count &&
						overridable == false;
					j++) {
					overridable =
						dataSet->b.b.overridable->items[j]
							.requiredPropertyIndex == i;
				}
			}
			EXPECT_TRUE(overridable) <<
				STRING(dataSet->b.b.available->items[i].name.data.ptr) <<
				" is a JavaScript property, so the engine can empty it, and "
				"it has to be one evidence can override for the list to be "
				"long enough.\n";
		}
	}
	EXCEPTION_THROW;
	EXPECT_GT(javaScriptProperties, 0) <<
		"The data file should carry at least one JavaScript property.\n";
	DataSetHashRelease(dataSet);
}

/*
 * Every value the evidence carries is applied, whatever the mix of headers
 * and results, now that the list is sized by the data set. See #411.
 */
TEST_F(HashCOverridesTests, EveryValueTheEvidenceCarriesIsApplied) {
	EXCEPTION_CREATE;
	vector<string> names = getOverridableNamesNotEmptied();
	if (names.empty()) {
		SUCCEED() << "The data file offers no properties that evidence can "
			"override without the engine emptying them again, so there is "
			"nothing to check.\n";
		return;
	}

	// One item of evidence for the User-Agent and one for each property the
	// data file lets evidence override. The keys are built before any is
	// added because the evidence does not copy them.
	vector<string> keys;
	for (size_t i = 0; i < names.size(); i++) {
		keys.push_back("51D_" + names[i]);
	}
	EvidenceKeyValuePairArray* evidence =
		EvidenceCreate((uint32_t)keys.size() + 1);
	EvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"User-Agent",
		mobileUserAgent);
	for (size_t i = 0; i < keys.size(); i++) {
		EvidenceAddString(
			evidence,
			FIFTYONE_DEGREES_EVIDENCE_QUERY,
			keys[i].c_str(),
			"1");
	}

	ResultsHash* results = ResultsHashCreate(&manager, 1);
	ResultsHashFromEvidence(results, evidence, exception);
	EXCEPTION_THROW;

	EXPECT_GE(results->b.b.overrides->count, (uint32_t)names.size()) <<
		"Every value the evidence carries should be held, along with the "
		"empty value for any JavaScript property measuring one of them.\n";
	char buffer[50] = "";
	for (size_t i = 0; i < names.size(); i++) {
		EXPECT_STREQ(
			"1",
			getPropertyValueAsString(
				results,
				names[i].c_str(),
				buffer,
				sizeof(buffer))) << "The value sent for " << names[i] <<
			" should be applied.\n";
	}

	ResultsHashFree(results);
	EvidenceFree(evidence);
}

/*
 * DeviceId is advertised as an available property and can be read from a
 * result, but it is not stored in the data file. Naming it in the required
 * properties must initialise, and produce the same device id as a data set
 * with no restriction on the required properties.
 */

static string getDeviceIdForUserAgent(
	ResourceManager *manager,
	const char *userAgent) {
	EXCEPTION_CREATE;
	char deviceId[80] = "";
	ResultsHash *results = ResultsHashCreate(manager, 0);
	ResultsHashFromUserAgent(
		results,
		userAgent,
		strlen(userAgent),
		exception);
	EXCEPTION_THROW;
	HashGetDeviceIdFromResults(results, deviceId, sizeof(deviceId), exception);
	EXCEPTION_THROW;
	ResultsHashFree(results);
	return string(deviceId);
}

class HashDeviceIdPropertyTests : public HashCTests {
public:
	void SetUp() { Base::SetUp(); }
	void TearDown() { Base::TearDown(); }
protected:
	StatusCode initManager(
		ResourceManager *deviceIdManager,
		PropertiesRequired *required) {
		EXCEPTION_CREATE;
		ConfigHash config = HashDefaultConfig;
		return HashInitManagerFromFile(
			deviceIdManager,
			&config,
			required,
			dataFilePath.c_str(),
			exception);
	}

	// The device id from a data set where every property is available, which
	// is what requesting DeviceId has to reproduce.
	string unrestrictedDeviceId() {
		ResourceManager deviceIdManager;
		PropertiesRequired required = PropertiesDefault;
		EXPECT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
		string deviceId = getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent);
		ResourceManagerFree(&deviceIdManager);
		return deviceId;
	}
};

TEST_F(HashDeviceIdPropertyTests, DeviceIdAsOnlyRequiredProperty) {
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required)) <<
		"DeviceId should be accepted as a required property.\n";
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent)) <<
		"Requesting only DeviceId should produce the same device id as "
		"requesting every property.\n";
	ResourceManagerFree(&deviceIdManager);
}

TEST_F(HashDeviceIdPropertyTests, DeviceIdIsCaseInsensitive) {
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "deviceid";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required)) <<
		"Property names are matched without regard to case.\n";
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));
	ResourceManagerFree(&deviceIdManager);
}

TEST_F(HashDeviceIdPropertyTests, DeviceIdFromArray) {
	ResourceManager deviceIdManager;
	const char *names[] = { "DeviceId" };
	PropertiesRequired required = PropertiesDefault;
	required.array = names;
	required.count = 1;
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));
	ResourceManagerFree(&deviceIdManager);
}

/*
 * A stored property requested alongside DeviceId must still be available, and
 * the components it does not cover must still be resolved.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdWithStoredProperty) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId,IsMobile";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	ManagerGuard managerGuard(&deviceIdManager, ResourceManagerFree);
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));

	OwnedResults results(
		ResultsHashCreate(&deviceIdManager, 0),
		ResultsHashFree);
	ResultsHashFromUserAgent(
		results.get(), mobileUserAgent, strlen(mobileUserAgent), exception);
	EXCEPTION_THROW;
	const int index = getRequiredPropertyIndex(results.get(), "IsMobile");
	ASSERT_GE(index, 0) << "IsMobile should still be available.\n";
	EXPECT_TRUE(ResultsHashGetHasValues(results.get(), index, exception));
	EXCEPTION_THROW;
}

/*
 * Unknown names are silently ignored by the properties source, so DeviceId
 * alongside one is the same as DeviceId alone.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdWithUnknownProperty) {
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId|NotARealProperty";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));
	ResourceManagerFree(&deviceIdManager);
}

/*
 * Requesting only names which are not in the data file must still fail. The
 * DeviceId handling must not make an entirely invalid list initialise.
 */
TEST_F(HashDeviceIdPropertyTests, UnknownPropertyStillFails) {
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "NotARealProperty";
	EXPECT_EQ(REQ_PROP_NOT_PRESENT, initManager(&deviceIdManager, &required));
}

/*
 * A reload takes the required property names from the available properties of
 * the data set being replaced, which never include DeviceId, so the device id
 * survives through the mark on that list, which the new list keeps.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdSurvivesReload) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	const string before = getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent);
	ASSERT_EQ(SUCCESS, HashReloadManagerFromFile(
		&deviceIdManager, dataFilePath.c_str(), exception));
	EXCEPTION_THROW;
	EXPECT_EQ(before, getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent)) <<
		"The device id should be unchanged by a reload.\n";
	ResourceManagerFree(&deviceIdManager);
}

TEST_F(HashDeviceIdPropertyTests, DeviceIdSurvivesReloadFromMemory) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	const string before = getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent);
	MemoryReader reader;
	ASSERT_EQ(SUCCESS, FileReadToByteArray(dataFilePath.c_str(), &reader));
	ASSERT_EQ(SUCCESS, HashReloadManagerFromMemory(
		&deviceIdManager, reader.startByte, reader.length, exception));
	EXCEPTION_THROW;
	EXPECT_EQ(before, getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent)) <<
		"The device id should be unchanged by a reload.\n";
	ResourceManagerFree(&deviceIdManager);
	Free(reader.startByte);
}

TEST_F(HashDeviceIdPropertyTests, DeviceIdWithStoredPropertySurvivesReload) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId,IsMobile";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));
	ASSERT_EQ(SUCCESS, HashReloadManagerFromFile(
		&deviceIdManager, dataFilePath.c_str(), exception));
	EXCEPTION_THROW;
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));
	DataSetHash *dataSet = (DataSetHash*)DataSetGet(&deviceIdManager);
	EXPECT_EQ(1u, dataSet->b.b.available->count);
	DataSetHashRelease(dataSet);
	ResourceManagerFree(&deviceIdManager);
}

/*
 * The device id must be produced without adding anything to the available
 * properties - the caller asked for one thing and must not be given others.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdAddsNoAvailableProperties) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));

	DataSetHash *dataSet = (DataSetHash*)DataSetGet(&deviceIdManager);
	EXPECT_EQ(0u, dataSet->b.b.available->count) <<
		"Requesting the device id should not make any property available.\n";
	EXPECT_EQ(dataSet->componentsList.count, dataSet->componentsAvailableCount)
		<< "Every component must be resolved to compose the device id.\n";
	DataSetHashRelease(dataSet);
	ResourceManagerFree(&deviceIdManager);
}

/*
 * The device id is the profile of every component joined together, so each
 * component must resolve to a real profile. A component which is skipped shows
 * up as a zero segment.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdResolvesEveryComponentProfile) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, initManager(&deviceIdManager, &required));

	ResultsHash *results = ResultsHashCreate(&deviceIdManager, 0);
	DataSetHash *dataSet = (DataSetHash*)results->b.b.dataSet;
	ResultsHashFromUserAgent(
		results, mobileUserAgent, strlen(mobileUserAgent), exception);
	EXCEPTION_THROW;
	ResultsEvaluateAll(&results->b.b, exception);
	EXCEPTION_THROW;

	const uint32_t nullProfileOffset = UINT32_MAX;
	for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
		bool resolved = false;
		for (uint32_t i = 0; i < results->count && resolved == false; i++) {
			resolved = results->items[i].profileOffsets[c] != nullProfileOffset;
		}
		EXPECT_TRUE(resolved) << "Component " << c <<
			" has no profile, so the device id would contain a zero.\n";
	}

	char deviceId[80] = "";
	HashGetDeviceIdFromResults(results, deviceId, sizeof(deviceId), exception);
	EXCEPTION_THROW;
	EXPECT_EQ(string::npos, ("-" + string(deviceId) + "-").find("-0-")) <<
		"No component of the device id should be zero: " << deviceId << "\n";
	EXPECT_EQ(unrestrictedDeviceId(), string(deviceId));

	ResultsHashFree(results);
	ResourceManagerFree(&deviceIdManager);
}

/*
 * Naming DeviceId marks that data set's own list of available properties, so
 * another data set made with the same config must not be affected.
 */
TEST_F(HashDeviceIdPropertyTests, NotCarriedToAnotherDataSet) {
	EXCEPTION_CREATE;
	ConfigHash config = HashDefaultConfig;
	PropertiesRequired required = PropertiesDefault;

	ResourceManager deviceIdManager;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, HashInitManagerFromFile(
		&deviceIdManager, &config, &required, dataFilePath.c_str(), exception));
	EXCEPTION_THROW;

	ResourceManager storedManager;
	required.string = "IsMobile";
	ASSERT_EQ(SUCCESS, HashInitManagerFromFile(
		&storedManager, &config, &required, dataFilePath.c_str(), exception));
	EXCEPTION_THROW;
	DataSetHash *dataSet = (DataSetHash*)DataSetGet(&storedManager);
	EXPECT_EQ(1u, dataSet->componentsAvailableCount) <<
		"Only the component of the stored property should be resolved.\n";
	DataSetHashRelease(dataSet);
	ResourceManagerFree(&storedManager);

	ResourceManager unknownManager;
	required.string = "NotARealProperty";
	EXPECT_EQ(REQ_PROP_NOT_PRESENT, HashInitManagerFromFile(
		&unknownManager, &config, &required, dataFilePath.c_str(), exception));

	ResourceManagerFree(&deviceIdManager);
}

/*
 * A config with a property value index builds the index from the available
 * properties, of which there are none when only the device id is required.
 */
TEST_F(HashDeviceIdPropertyTests, DeviceIdWithPropertyValueIndex) {
	EXCEPTION_CREATE;
	ResourceManager deviceIdManager;
	ConfigHash config = HashInMemoryConfig;
	ASSERT_TRUE(config.b.b.propertyValueIndex);
	PropertiesRequired required = PropertiesDefault;
	required.string = "DeviceId";
	ASSERT_EQ(SUCCESS, HashInitManagerFromFile(
		&deviceIdManager, &config, &required, dataFilePath.c_str(), exception));
	EXCEPTION_THROW;
	EXPECT_EQ(unrestrictedDeviceId(),
		getDeviceIdForUserAgent(&deviceIdManager, mobileUserAgent));
	ResourceManagerFree(&deviceIdManager);
}

/*
 * The query value of 51D_GetHighEntropyValues and 51D_StructuredUserAgent
 * takes precedence over the cookie value, whatever order the evidence is
 * given in. The evidence is built through the C API so that the order is the
 * one the caller chose, which a map keyed by name cannot express.
 */
class HashSpecialEvidencePrecedenceTests : public HashCTests {
public:
	void SetUp() {
		Base::SetUp();
		properties.string = "PlatformName,PlatformVersion";
		EXCEPTION_CREATE;
		HashInitManagerFromFile(
			&manager,
			&configHash,
			&properties,
			dataFilePath.c_str(),
			exception);
		EXCEPTION_THROW;
	}
	void TearDown() {
		ResourceManagerFree(&manager);
		Base::TearDown();
	}
protected:
	struct Item {
		EvidencePrefix prefix;
		const char *key;
		const char *value;
	};

	// The platform version from the evidence, with the User-Agent as a header
	// and then the items in the order given.
	string platformVersion(const vector<Item> &items) {
		EXCEPTION_CREATE;
		EvidenceKeyValuePairArray *evidence = EvidenceCreate(
			(uint32_t)items.size() + 1);
		EvidenceAddString(
			evidence,
			FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
			"User-Agent",
			windowsUserAgent);
		for (const Item &item : items) {
			EvidenceAddString(evidence, item.prefix, item.key, item.value);
		}
		ResultsHash *results = ResultsHashCreate(&manager, 0);
		ResultsHashFromEvidence(results, evidence, exception);
		EXCEPTION_THROW;
		char buffer[256];
		string version = getPropertyValueAsString(
			results, "PlatformVersion", buffer, sizeof(buffer));
		ResultsHashFree(results);
		EvidenceFree(evidence);
		return version;
	}

	// Checks the query value gives the Windows 11 platform version when the
	// cookie value, which gives another, is added before and after it.
	void expectQueryWins(
		const char *queryKey,
		const char *queryValue,
		const char *cookieKey,
		const char *cookieValue) {
		const Item query = {
			FIFTYONE_DEGREES_EVIDENCE_QUERY, queryKey, queryValue };
		const Item cookie = {
			FIFTYONE_DEGREES_EVIDENCE_COOKIE, cookieKey, cookieValue };

		ASSERT_EQ(windows11Version, platformVersion({ query })) <<
			"The query value alone must give the Windows 11 version.\n";
		ASSERT_NE(windows11Version, platformVersion({ cookie })) <<
			"The cookie value alone must give another version for this "
			"test to show anything.\n";

		EXPECT_EQ(windows11Version, platformVersion({ query, cookie })) <<
			"Query added before the cookie, the query should win.\n";
		EXPECT_EQ(windows11Version, platformVersion({ cookie, query })) <<
			"Cookie added before the query, the query should win.\n";
	}

	const char *windowsUserAgent =
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
		"(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";
	const string windows11Version = "11.0";

	// Platform version 15.0.0, which the data file maps to Windows 11.
	const char *windows11Ghev =
		"eyJicmFuZHMiOlt7ImJyYW5kIjoiQ2hyb21pdW0iLCJ2ZXJzaW9uIjoiMTUyIn0seyJicmFu"
		"ZCI6Ikdvb2dsZSBDaHJvbWUiLCJ2ZXJzaW9uIjoiMTUyIn0seyJicmFuZCI6Ik5vdD9BX0Jy"
		"YW5kIiwidmVyc2lvbiI6IjI0In1dLCJmdWxsVmVyc2lvbkxpc3QiOlt7ImJyYW5kIjoiQ2hy"
		"b21pdW0iLCJ2ZXJzaW9uIjoiMTUyLjAuNzI1OC42NiJ9LHsiYnJhbmQiOiJHb29nbGUgQ2hy"
		"b21lIiwidmVyc2lvbiI6IjE1Mi4wLjcyNTguNjYifSx7ImJyYW5kIjoiTm90P0FfQnJhbmQi"
		"LCJ2ZXJzaW9uIjoiMjQuMC4wLjAifV0sIm1vYmlsZSI6ZmFsc2UsIm1vZGVsIjoiIiwicGxh"
		"dGZvcm0iOiJXaW5kb3dzIiwicGxhdGZvcm1WZXJzaW9uIjoiMTUuMC4wIn0=";

	// The same values with platform version 10.0.0.
	const char *windows10Ghev =
		"eyJicmFuZHMiOlt7ImJyYW5kIjoiQ2hyb21pdW0iLCJ2ZXJzaW9uIjoiMTUyIn0seyJicmFu"
		"ZCI6Ikdvb2dsZSBDaHJvbWUiLCJ2ZXJzaW9uIjoiMTUyIn0seyJicmFuZCI6Ik5vdD9BX0Jy"
		"YW5kIiwidmVyc2lvbiI6IjI0In1dLCJmdWxsVmVyc2lvbkxpc3QiOlt7ImJyYW5kIjoiQ2hy"
		"b21pdW0iLCJ2ZXJzaW9uIjoiMTUyLjAuNzI1OC42NiJ9LHsiYnJhbmQiOiJHb29nbGUgQ2hy"
		"b21lIiwidmVyc2lvbiI6IjE1Mi4wLjcyNTguNjYifSx7ImJyYW5kIjoiTm90P0FfQnJhbmQi"
		"LCJ2ZXJzaW9uIjoiMjQuMC4wLjAifV0sIm1vYmlsZSI6ZmFsc2UsIm1vZGVsIjoiIiwicGxh"
		"dGZvcm0iOiJXaW5kb3dzIiwicGxhdGZvcm1WZXJzaW9uIjoiMTAuMC4wIn0=";

	// Structured User-Agent values, which are JSON and not encoded. Platform
	// version 14.0.0 is Windows 11 and 10.0.0 is Windows 10.
	const char *windows11Sua =
		"{\"browsers\":[{\"brand\":\"Chromium\",\"version\":[\"140\",\"0\","
		"\"0\",\"0\"]},{\"brand\":\"Google Chrome\",\"version\":[\"140\","
		"\"0\",\"0\",\"0\"]}],\"platform\":{\"brand\":\"Windows\","
		"\"version\":[\"14\",\"0\",\"0\"]},\"mobile\":0,"
		"\"architecture\":\"x86\",\"source\":2}";
	const char *windows10Sua =
		"{\"browsers\":[{\"brand\":\"Chromium\",\"version\":[\"140\",\"0\","
		"\"0\",\"0\"]},{\"brand\":\"Google Chrome\",\"version\":[\"140\","
		"\"0\",\"0\",\"0\"]}],\"platform\":{\"brand\":\"Windows\","
		"\"version\":[\"10\",\"0\",\"0\"]},\"mobile\":0,"
		"\"architecture\":\"x86\",\"source\":2}";
};

TEST_F(HashSpecialEvidencePrecedenceTests, GhevQueryBeatsCookie) {
	expectQueryWins(
		"51D_GetHighEntropyValues", windows11Ghev,
		"51D_GetHighEntropyValues", windows10Ghev);
}

TEST_F(HashSpecialEvidencePrecedenceTests, SuaQueryBeatsCookie) {
	expectQueryWins(
		"51D_StructuredUserAgent", windows11Sua,
		"51D_StructuredUserAgent", windows10Sua);
}

TEST_F(HashSpecialEvidencePrecedenceTests, QuerySuaBeatsCookieGhev) {
	expectQueryWins(
		"51D_StructuredUserAgent", windows11Sua,
		"51D_GetHighEntropyValues", windows10Ghev);
}

TEST_F(HashSpecialEvidencePrecedenceTests, QueryGhevBeatsCookieSua) {
	expectQueryWins(
		"51D_GetHighEntropyValues", windows11Ghev,
		"51D_StructuredUserAgent", windows10Sua);
}

TEST_F(HashSpecialEvidencePrecedenceTests, CookieGhevUsedAlone) {
	EXPECT_EQ(windows11Version, platformVersion({ {
		FIFTYONE_DEGREES_EVIDENCE_COOKIE,
		"51D_GetHighEntropyValues",
		windows11Ghev } }));
}

TEST_F(HashSpecialEvidencePrecedenceTests, CookieSuaUsedAlone) {
	EXPECT_EQ(windows11Version, platformVersion({ {
		FIFTYONE_DEGREES_EVIDENCE_COOKIE,
		"51D_StructuredUserAgent",
		windows11Sua } }));
}

// A query value that cannot be decoded gives no header, so the cookie value
// is still used, whichever order the two were added in.
TEST_F(HashSpecialEvidencePrecedenceTests, InvalidQueryGhevLeavesTheCookie) {
	const Item query = {
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		"51D_GetHighEntropyValues",
		"!!not a base64 value!!" };
	const Item cookie = {
		FIFTYONE_DEGREES_EVIDENCE_COOKIE,
		"51D_GetHighEntropyValues",
		windows11Ghev };
	ASSERT_NE(windows11Version, platformVersion({ query })) <<
		"The invalid query value alone must not give the Windows 11 "
		"version.\n";
	EXPECT_EQ(windows11Version, platformVersion({ query, cookie })) <<
		"Invalid query added before the cookie, the cookie should be used.\n";
	EXPECT_EQ(windows11Version, platformVersion({ cookie, query })) <<
		"Cookie added before the invalid query, the cookie should be used.\n";
}

TEST_F(HashSpecialEvidencePrecedenceTests, InvalidQuerySuaLeavesTheCookie) {
	const Item query = {
		FIFTYONE_DEGREES_EVIDENCE_QUERY,
		"51D_StructuredUserAgent",
		"{not valid json" };
	const Item cookie = {
		FIFTYONE_DEGREES_EVIDENCE_COOKIE,
		"51D_StructuredUserAgent",
		windows11Sua };
	ASSERT_NE(windows11Version, platformVersion({ query })) <<
		"The invalid query value alone must not give the Windows 11 "
		"version.\n";
	EXPECT_EQ(windows11Version, platformVersion({ query, cookie })) <<
		"Invalid query added before the cookie, the cookie should be used.\n";
	EXPECT_EQ(windows11Version, platformVersion({ cookie, query })) <<
		"Cookie added before the invalid query, the cookie should be used.\n";
}

/*
 * Check that the reasons the typed readers give without a value each have
 * their own message, and not the one for an unknown reason.
 */
TEST(HashNoValueMessages, EachReasonHasItsOwnMessage) {
	const char *unknown = fiftyoneDegreesResultsHashGetNoValueReasonMessage(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_UNKNOWN);
	const fiftyoneDegreesResultsNoValueReason reasons[] = {
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NO_RESULT_FOR_PROPERTY,
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_TOO_MANY_VALUES };
	for (fiftyoneDegreesResultsNoValueReason reason : reasons) {
		const char *message =
			fiftyoneDegreesResultsHashGetNoValueReasonMessage(reason);
		ASSERT_NE(nullptr, message) << (int)reason;
		EXPECT_STRNE("", message) << (int)reason;
		EXPECT_STRNE(unknown, message) << (int)reason;
	}
}
