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
#include "EvidenceTests.hpp"
#include "memory.h"
#include "../../src/common/EvidenceBase.hpp"
#include "../../src/common/fiftyone.h"

using namespace FiftyoneDegrees::Common;

void assertStringHeaderAdded(
	fiftyoneDegreesEvidenceKeyValuePair *pair,
	const char *expectedField,
	const char *expectedValue) {
	EXPECT_EQ((int)pair->prefix, (int)FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING) <<
		L"Expected 'header' prefix.";
	EXPECT_STREQ(pair->item.key, expectedField) <<
		L"Expected name '" << expectedField << "' not '" << pair->item.key << "'";
	EXPECT_TRUE(strcmp((const char*)pair->item.value, expectedValue) == 0) <<
		L"Expected value '" << expectedValue << "' not '" << pair->item.value << "'";
}

TEST_F(Evidence, Get_PrefixString) {
	struct {
		fiftyoneDegreesEvidencePrefix prefix;
		const char* expected;
	} testData[4] = {
		{FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "header."},
		{FIFTYONE_DEGREES_EVIDENCE_SERVER, "server."},
		{FIFTYONE_DEGREES_EVIDENCE_QUERY, "query."},
		{FIFTYONE_DEGREES_EVIDENCE_COOKIE, "cookie."}
	};

	for (int i = 0; i < 4; i++) {
		const char* prefixStr = fiftyoneDegreesEvidencePrefixString(
			testData[i].prefix);
		EXPECT_TRUE(
			strcmp(
				testData[i].expected,
				prefixStr) == 0) <<
			L"Expected prefix string " << testData[i].expected << " "
			L"For prefix " << testData[i].prefix << " "
			L"But get " << prefixStr << ".";
	}
}

/*
 * Check that a single string can be added to evidence.
 */
TEST_F(Evidence, Add_SingleString) {
	CreateEvidence(1);
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"some-header-name",
		"some-header-value");
	assertStringHeaderAdded(&evidence->items[0], "some-header-name", "some-header-value");
}

/*
 * Check that multiple strings can be added to evidence.
 */
TEST_F(Evidence, Add_MultipleStrings)
{
	CreateEvidence(2);
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"some-header-name",
		"some-header-value");
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"some-other-header-name",
		"some-header-value");
	assertStringHeaderAdded(&evidence->items[0], "some-header-name", "some-header-value");
	assertStringHeaderAdded(&evidence->items[1], "some-other-header-name", "some-header-value");
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
bool onMatchIterateString(void *state, fiftyoneDegreesEvidenceKeyValuePair *pair)
{
	if (strcmp((const char*)pair->item.key, "some-header-name") == 0) {
		EXPECT_TRUE(strcmp((const char*)pair->item.value, (const char*)pair->parsedValue) == 0) <<
			L"Expected parsed value to be '" << (const char*)pair->item.value << "' not '" <<
			(const char*)pair->parsedValue << "'";
	}
	return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/*
 *Check that the parsed version of a string evidence value will be the same string.
 */
TEST_F(Evidence, Iterate_String)
{
	CreateEvidence(1);
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"some-header-name",
		"some-header-value");
	evidence->items[0].parsedValue = NULL;
	fiftyoneDegreesEvidenceIterate(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		NULL, 
		onMatchIterateString);
}


const char* parsedValue = "already-parsed";
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
bool onMatchIterateStringAlreadyParsed(void *state, fiftyoneDegreesEvidenceKeyValuePair *pair)
{
	EXPECT_TRUE(strcmp(parsedValue, (const char*)pair->parsedValue) == 0) <<
		L"Expected parsed value to be '" << parsedValue << "' not '" <<
		(const char*)pair->parsedValue << "'";
	return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/*
 * Check that an evidence value is not parsed again if it has already been parsed.
 */
TEST_F(Evidence, Iterate_String_AlreadyParsed) {
	CreateEvidence(1);
	char* parsed = (char*)malloc(sizeof(char) * (strlen(parsedValue) + 1));
	strcpy(parsed, parsedValue);
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"some-header-name",
		"some-header-value");
	evidence->items[0].parsedValue = parsed;
	fiftyoneDegreesEvidenceIterate(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		NULL,
		onMatchIterateStringAlreadyParsed);
	free(parsed);
}

#ifdef _MSC_VER
// This is mock implementation of the method so not all arguments are used
#pragma warning (disable: 4100)
#endif
static bool countEvidence(
	void* state,
	fiftyoneDegreesEvidenceKeyValuePair* pair) {
	(*(int*)state)++;
	return true;
}
#ifdef _MSC_VER
#pragma warning (default: 4100)
#endif

/*
 * Check that the iteration API only iterate through the main evidence list
 */
TEST_F(Evidence, Iterate_String_without_pseudo_evidence) {
	CreateEvidence(1);
	fiftyoneDegreesEvidenceAddString(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		"header1",
		"value1");

	int count = 0;
	fiftyoneDegreesEvidenceIterate(
		evidence,
		FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
		&count,
		countEvidence);

	EXPECT_EQ(1, count) <<
		"Number of evidence should be 1\n";
}

bool callback1(void* state, fiftyoneDegreesEvidenceKeyValuePair* pair) {
    std::vector<std::string> *results = (std::vector<std::string> *) state;
    results->push_back(pair->item.value);
    return true;
}

TEST_F(Evidence, MapPrefix) {
    auto map = fiftyoneDegreesEvidenceMapPrefix("server.param1");
    EXPECT_EQ(map->prefixEnum, FIFTYONE_DEGREES_EVIDENCE_SERVER);
    map = fiftyoneDegreesEvidenceMapPrefix("query.PARAM");
    EXPECT_EQ(map->prefixEnum, FIFTYONE_DEGREES_EVIDENCE_QUERY);
    map = fiftyoneDegreesEvidenceMapPrefix("cookie.some_value");
    EXPECT_EQ(map->prefixEnum, FIFTYONE_DEGREES_EVIDENCE_COOKIE);
    map = fiftyoneDegreesEvidenceMapPrefix("header.HTTP_STRING");
    EXPECT_EQ(map->prefixEnum, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING);
    map = fiftyoneDegreesEvidenceMapPrefix("header."); // a string without a param does not have a prefix NULL
    EXPECT_EQ(map, nullptr);
    map = fiftyoneDegreesEvidenceMapPrefix("nonsense");
    EXPECT_EQ(map, nullptr);
    map = fiftyoneDegreesEvidenceMapPrefix("HEADER.something"); // case-sensitivity of the prefixes
    EXPECT_EQ(map, nullptr);
}
                                                          
TEST_F(Evidence, IterateForHeaders_Order) {
    const char *headers[] = {
        (char *)"Material",
        (char *)"Size",
        (char *)"Color"
    };
    headersContainer.CreateHeaders(headers, 3, false);
    CreateEvidence(3);
    
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");
    
    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results[0], "Apple");
    EXPECT_EQ(results[1], "Big");
    EXPECT_EQ(results[2], "Green");
}

TEST_F(Evidence, IterateForHeaders_NoMatchingPrefix) {
    const char *headers[] = {
        (char *)"Material",
        (char *)"Size",
        (char *)"Color"
    };
    headersContainer.CreateHeaders(headers, 3, false);
    CreateEvidence(3);
    
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "Material", "Apple");
    
    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results[0], "Green"); // only Color is the header string
    EXPECT_EQ(results.size(), 1);
}

TEST_F(Evidence, IterateForHeaders_PrefixPrecedence) {
    const char *headers[] = {
        (char *)"Material",
        (char *)"Size",
        (char *)"Color"
    };
    headersContainer.CreateHeaders(headers, 3, false);
    CreateEvidence(7);
    
    // at this level no prefix precedence is enforced -
    // the first evidence matching the prefix mask will be used
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "BigHeader");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_COOKIE, "Size", "BigCookie");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "Size", "BigQuery");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_QUERY, "Material", "AppleQuery");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_COOKIE, "Material", "AppleCookie");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "AppleHeader");
    
    
    std::vector<std::string> results;
    uint32_t prefixMask = FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING | FIFTYONE_DEGREES_EVIDENCE_QUERY | FIFTYONE_DEGREES_EVIDENCE_COOKIE;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, prefixMask, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], "AppleQuery"); // only Color is the header string
    EXPECT_EQ(results[1], "BigHeader"); // only Color is the header string
    EXPECT_EQ(results[2], "Green"); // only Color is the header string
}

TEST_F(Evidence, IterateForHeaders_MissingOneEvidence) {
    const char *headers[] = {
        (char *)"Material",
        (char *)"Size",
        (char *)"Color"
    };
    headersContainer.CreateHeaders(headers, 3, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");
    
    
    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results[0], "Apple");
    EXPECT_EQ(results[1], "Green");
}

TEST_F(Evidence, IterateForHeaders_ConstructPseudoHeader) {
    const char *headers[] = {
        "Material",
        "Size\x1FTaste", //Taste is a missing evidence, this pseudoheader should not be constructed
        "Size\x1F""Color",
        "Size\x1F""Color\x1F""Material",
        
    };
    headersContainer.CreateHeaders(headers, 4, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");
    

    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], "Apple");
    EXPECT_EQ(results[1], "Big\x1FGreen");
    EXPECT_EQ(results[2], "Big\x1FGreen\x1F""Apple");
}

TEST_F(Evidence, IterateForHeaders_ConstructPseudoHeader_Missing_or_Empty) {
    const char *headers[] = {
        "Taste\x1FObject", //Taste is a missing evidence, this pseudoheader should not be constructed
        "Size\x1FTaste", //Taste is a missing evidence, this pseudoheader should not be constructed
        "Size\x1F""Color", //Color is going to be empty evidence, so this header is to be constructed
        "Size\x1F""Color\x1F""Object",
    };
    
    headersContainer.CreateHeaders(headers, 4, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Object", "Apple");
    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], "\x1FGreen");
    EXPECT_EQ(results[1], "\x1FGreen\x1F""Apple");
}

bool callback2(void* state, fiftyoneDegreesEvidenceKeyValuePair *pair) {
    std::vector<std::string> *results = (std::vector<std::string> *) state;
    results->push_back(pair->item.value);
    return results->size() <= 1; // on the second header we signal early exit by returning false
}

TEST_F(Evidence, IterateForHeaders_CallbackSignalsEarlyExit) {
    const char *headers[] = {
        "Material",
        "Size\x1FTaste", //Taste is a missing evidence, this pseudoheader should not be constructed
        "Size\x1F""Color",
        "Size\x1F""Color\x1F""Material",
    };
    headersContainer.CreateHeaders(headers, 4, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");
    
    std::vector<std::string> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, &results, callback2);
    EXPECT_TRUE(res);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], "Apple");
    EXPECT_EQ(results[1], "Big\x1FGreen");
}

bool callback_false(void*, fiftyoneDegreesEvidenceKeyValuePair*) {
    return false;
}

TEST_F(Evidence, IterateForHeaders_CallbackSignalsEarlyExit_OrdinaryHeader) {
    const char *headers[] = {
        "Material"
    };
    headersContainer.CreateHeaders(headers, 1, false);
    CreateEvidence(1);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buffer, bufferSize, NULL, callback_false);
    EXPECT_TRUE(res);
}

TEST_F(Evidence, IterateForHeaders_SmallBuffer) {
    const char *headers[] = {
        "Size\x1F""Color",
        "Size\x1F""Color\x1F""Material",
    };
    headersContainer.CreateHeaders(headers, 2, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");

    std::vector<std::string> results;
    
    // Big\x1FGreen\x1FApple => 15 characters, buffer size must be 16,
    // let's make it exactly 15, and the last result is cut to the 14
    // characters the buffer holds before the null terminator

    size_t length = 15;
    char *buf = (char *) fiftyoneDegreesMalloc(length);
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buf, length, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], "Big\x1F""Green");
    EXPECT_EQ(results[1], "Big\x1F""Green\x1F""Appl");
    fiftyoneDegreesFree(buf);
    
    // now let's make it 16 and it should be formed
    results.clear();
    length = 16;
    buf = (char *) fiftyoneDegreesMalloc(length);
    res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, headersContainer.headerPointers, buf, length, &results, callback1);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[1], "Big\x1F""Green\x1F""Apple");
    fiftyoneDegreesFree(buf);
}

TEST_F(Evidence, freeNullEvidence) {
    fiftyoneDegreesEvidenceKeyValuePairArray *evidence2 = NULL;
    EvidenceFree(evidence2);
}

TEST_F(Evidence, emptyEvidence) {
    EvidenceBase emptyEvidence;
    //this produces an empty array, but items pointer is set to 1 past the end of array structure,
    //so we do not always segfault if we attempt to iterate over it
    //address sanitizer always reveals this heap overflow however
    fiftyoneDegreesEvidenceKeyValuePairArray *emptyEvidenceKVPA = emptyEvidence.get();

    std::vector<std::string> results;
    auto iterations = EvidenceIterate(emptyEvidenceKVPA, FIFTYONE_DEGREES_EVIDENCE_QUERY,
     &results, callback1);

    EXPECT_EQ(iterations, 0);
}

struct PseudoHeaderLengthResult {
    std::string value;
    size_t valueLength;
    std::string parsedValue;
    size_t parsedLength;
};

bool callbackValidateLengths(void* state, fiftyoneDegreesEvidenceKeyValuePair* pair) {
    std::vector<PseudoHeaderLengthResult>* results =
        (std::vector<PseudoHeaderLengthResult>*) state;
    PseudoHeaderLengthResult result;
    result.value = std::string((const char*)pair->item.value);
    result.valueLength = pair->item.valueLength;
    result.parsedValue = std::string((const char*)pair->parsedValue);
    result.parsedLength = pair->parsedLength;
    results->push_back(result);
    return true;
}

TEST_F(Evidence, IterateForHeaders_PseudoHeader_Lengths) {
    const char *headers[] = {
        "Size\x1F""Color",
    };
    headersContainer.CreateHeaders(headers, 1, false);
    CreateEvidence(2);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");

    std::vector<PseudoHeaderLengthResult> results;
    fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
        headersContainer.headerPointers, buffer, bufferSize, &results, callbackValidateLengths);

    EXPECT_EQ(results.size(), 1);
    // "Big\x1FGreen" = 9 characters
    EXPECT_EQ(results[0].value, "Big\x1FGreen");
    EXPECT_EQ(results[0].valueLength, strlen(results[0].value.c_str()));
    EXPECT_EQ(results[0].valueLength, 9);
    EXPECT_EQ(results[0].parsedValue, "Big\x1FGreen");
    EXPECT_EQ(results[0].parsedLength, strlen(results[0].parsedValue.c_str()));
    EXPECT_EQ(results[0].parsedLength, 9);
}

// A pseudo header whose values together are longer than the buffer reaches the
// callback with the characters the buffer holds, with lengths to match, and
// the headers after it are still processed.
TEST_F(Evidence, IterateForHeaders_PseudoHeader_LongerThanBuffer) {
    const char *headers[] = {
        "Size\x1F""Color",
        "Material",
    };
    headersContainer.CreateHeaders(headers, 2, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "GreenWithAVeryLongName");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");

    // "Big\x1FGreenWithAVeryLongName" is 26 characters. A buffer of 16 holds
    // the first 15 and the null terminator.
    const size_t length = 16;
    char *buf = (char *)fiftyoneDegreesMalloc(length);
    std::vector<PseudoHeaderLengthResult> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
        headersContainer.headerPointers, buf, length, &results, callbackValidateLengths);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].value, "Big\x1FGreenWithAV");
    EXPECT_EQ(results[0].valueLength, 15);
    EXPECT_EQ(results[0].parsedValue, "Big\x1FGreenWithAV");
    EXPECT_EQ(results[0].parsedLength, 15);
    EXPECT_EQ(results[1].value, "Apple");
    EXPECT_EQ(results[1].valueLength, 5);
    fiftyoneDegreesFree(buf);
}

// The first segment alone can be longer than the buffer. The pseudo header is
// still passed on, holding what fits of that segment.
TEST_F(Evidence, IterateForHeaders_PseudoHeader_FirstSegmentLongerThanBuffer) {
    const char *headers[] = {
        "Size\x1F""Color",
    };
    headersContainer.CreateHeaders(headers, 1, false);
    CreateEvidence(2);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Enormous");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");

    const size_t length = 5;
    char *buf = (char *)fiftyoneDegreesMalloc(length);
    std::vector<PseudoHeaderLengthResult> results;
    fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
        headersContainer.headerPointers, buf, length, &results, callbackValidateLengths);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, "Enor");
    EXPECT_EQ(results[0].valueLength, 4);
    EXPECT_EQ(results[0].parsedLength, 4);
    fiftyoneDegreesFree(buf);
}

// The buffer can run out straight after a separator, which leaves the separator
// as the last character of the value.
TEST_F(Evidence, IterateForHeaders_PseudoHeader_CutAfterSeparator) {
    const char *headers[] = {
        "Size\x1F""Color",
    };
    headersContainer.CreateHeaders(headers, 1, false);
    CreateEvidence(2);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");

    const size_t length = 5;
    char *buf = (char *)fiftyoneDegreesMalloc(length);
    std::vector<PseudoHeaderLengthResult> results;
    fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
        headersContainer.headerPointers, buf, length, &results, callbackValidateLengths);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, "Big\x1F");
    EXPECT_EQ(results[0].valueLength, 4);
    EXPECT_EQ(results[0].parsedLength, 4);
    fiftyoneDegreesFree(buf);
}

// A zero length buffer has no room for the null terminator, so the pseudo header
// is not assembled, as with a null buffer, and nothing is written to the buffer.
// The headers after it are still processed.
TEST_F(Evidence, IterateForHeaders_PseudoHeader_ZeroLengthBuffer) {
    const char *headers[] = {
        "Size\x1F""Color",
        "Material",
    };
    headersContainer.CreateHeaders(headers, 2, false);
    CreateEvidence(3);
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Size", "Big");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Color", "Green");
    fiftyoneDegreesEvidenceAddString(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING, "Material", "Apple");

    char buf[1] = { 'x' };
    std::vector<PseudoHeaderLengthResult> results;
    bool res = fiftyoneDegreesEvidenceIterateForHeaders(evidence, FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
        headersContainer.headerPointers, buf, 0, &results, callbackValidateLengths);
    EXPECT_FALSE(res);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, "Apple");
    EXPECT_EQ(buf[0], 'x');
}
