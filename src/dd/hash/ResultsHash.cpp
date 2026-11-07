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

#include "ResultsHash.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees;

#define RESULT(r,i) ((ResultHash*)r->b.b.items + i)

DeviceDetection::Hash::ResultsHash::ResultsHash(
	fiftyoneDegreesResultsHash *results,
	shared_ptr<fiftyoneDegreesResourceManager> manager)
	: ResultsDeviceDetection(&results->b, manager) {
	this->results = results;
	_jsHardwareProfileRequiredIndex =
		PropertiesGetRequiredPropertyIndexFromName(
			this->available,
			"javascripthardwareprofile");
}


DeviceDetection::Hash::ResultsHash::~ResultsHash() {
	ResultsHashFree(results);
}

void DeviceDetection::Hash::ResultsHash::getValuesInternal(
	int requiredPropertyIndex,
	vector<string> &values) {
	EXCEPTION_CREATE;
	uint32_t i;
	Item *valuesItems;
	String *valueName;
	
	// Get a pointer to the first value item for the property.
	valuesItems = ResultsHashGetValues(
		results,
		requiredPropertyIndex,
		exception);
	EXCEPTION_THROW;

	if (valuesItems == NULL) {
		// No pointer to values was returned. 
		throw NoValuesAvailableException();
	}

	// Set enough space in the vector for all the strings that will be 
	// inserted.
	values.reserve(results->values.count);

	if (_jsHardwareProfileRequiredIndex >= 0 &&
		(int)requiredPropertyIndex == _jsHardwareProfileRequiredIndex) {

		// Add the values as JavaScript snippets to the result.
		for (i = 0; i < results->values.count; i++) {
			valueName = (String*)valuesItems[i].data.ptr;
			if (valueName != nullptr) {
				stringstream stream;
				stream <<
					"var profileIds = []\n" <<
					STRING(valueName) <<
					"\ndocument.cookie = \"51D_ProfileIds=\" + " <<
					"profileIds.join(\"|\")";
				values.push_back(stream.str());
			}
		}
	}
	else {

		// Add the values in their original form to the result.
		for (i = 0; i < results->values.count; i++) {
			valueName = (String*)valuesItems[i].data.ptr;
			if (valueName != nullptr)
			{
				values.push_back(string(STRING(valueName)));
			}
		}
	}
}

bool DeviceDetection::Hash::ResultsHash::hasValuesInternal(
	int requiredPropertyIndex) {
	EXCEPTION_CREATE;
	bool hasValues = fiftyoneDegreesResultsHashGetHasValues(
		results,
		requiredPropertyIndex,
		exception);
	EXCEPTION_THROW;
	return hasValues;
}

const char* DeviceDetection::Hash::ResultsHash::getNoValueMessageInternal(
	fiftyoneDegreesResultsNoValueReason reason) {
	return fiftyoneDegreesResultsHashGetNoValueReasonMessage(reason);
}

fiftyoneDegreesResultsNoValueReason
DeviceDetection::Hash::ResultsHash::getNoValueReasonInternal(
	int requiredPropertyIndex) {
	EXCEPTION_CREATE;
	fiftyoneDegreesResultsNoValueReason reason =
		fiftyoneDegreesResultsHashGetNoValueReason(
			results,
			requiredPropertyIndex,
			exception);
	EXCEPTION_THROW;
	return reason;
}

void DeviceDetection::Hash::ResultsHash::evaluateAll() const {
	EXCEPTION_CREATE;
	ResultsEvaluateAll(&results->b.b, exception);
	EXCEPTION_THROW;
}

string DeviceDetection::Hash::ResultsHash::getDeviceId(
	uint32_t resultIndex) const {
	EXCEPTION_CREATE;
	char deviceId[50] = "";
	evaluateAll();
	if (resultIndex < results->count) {
		HashGetDeviceIdFromResult(
			(DataSetHash*)results->b.b.dataSet,
			&results->items[resultIndex],
			deviceId,
			sizeof(deviceId),
			exception);
		EXCEPTION_THROW;
	}
	return string(deviceId);
}

string DeviceDetection::Hash::ResultsHash::getDeviceId() const {
	EXCEPTION_CREATE;
	char deviceId[50] = "";
	HashGetDeviceIdFromResults(
		results,
		deviceId,
		sizeof(deviceId),
		exception);
	EXCEPTION_THROW;
	return string(deviceId);
}

int DeviceDetection::Hash::ResultsHash::getIterations() const {
	EXCEPTION_CREATE;
	int32_t iterations;
	ResultsHashGetMatchMetrics(
		results,
		nullptr,
		nullptr,
		nullptr,
		&iterations,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return iterations;
}

int DeviceDetection::Hash::ResultsHash::getMatchedNodes() const {
	EXCEPTION_CREATE;
	int32_t matchedNodes;
	ResultsHashGetMatchMetrics(
		results,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		&matchedNodes,
		exception);
	EXCEPTION_THROW;
	return matchedNodes;
}

int DeviceDetection::Hash::ResultsHash::getDrift(uint32_t resultIndex) const {
	EXCEPTION_CREATE;
	int32_t drift;
	ResultsHashGetMatchMetricsForResult(
		results,
		resultIndex,
		nullptr,
		nullptr,
		&drift,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return drift;
}

int DeviceDetection::Hash::ResultsHash::getDrift() const {
	EXCEPTION_CREATE;
	int32_t drift;
	ResultsHashGetMatchMetrics(
		results,
		nullptr,
		nullptr,
		&drift,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return drift;
}

int DeviceDetection::Hash::ResultsHash::getDifference(
	uint32_t resultIndex) const {
	EXCEPTION_CREATE;
	int32_t difference;
	ResultsHashGetMatchMetricsForResult(
		results,
		resultIndex,
		nullptr,
		&difference,
		nullptr,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return difference;
}

int DeviceDetection::Hash::ResultsHash::getDifference() const {
	EXCEPTION_CREATE;
	int32_t difference;
	ResultsHashGetMatchMetrics(
		results,
		nullptr,
		&difference,
		nullptr,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return difference;
}

int DeviceDetection::Hash::ResultsHash::getMethod(uint32_t resultIndex) const {
	EXCEPTION_CREATE;
	HashMatchMethod method;
	ResultsHashGetMatchMetricsForResult(
		results,
		resultIndex,
		&method,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return method;
}

int DeviceDetection::Hash::ResultsHash::getMethod() const {
	EXCEPTION_CREATE;
	HashMatchMethod method;
	ResultsHashGetMatchMetrics(
		results,
		&method,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		exception);
	EXCEPTION_THROW;
	return method;
}

string DeviceDetection::Hash::ResultsHash::getTrace(uint32_t resultIndex) const {
	string trace;
	char *traceStr;
	int length;
	evaluateAll();
	if (resultIndex < results->count) {
		if (results->items[resultIndex].trace != nullptr) {
			length = GraphTraceGet(
				nullptr,
				0,
				results->items[resultIndex].trace,
				nullptr);
			traceStr = (char*)Malloc(length * sizeof(char));
			GraphTraceGet(
				traceStr,
				length,
				results->items[resultIndex].trace,
				results->items[resultIndex].b.targetUserAgent);
			trace.assign(traceStr);
			Free(traceStr);
		}
	}
	return trace;
}

string DeviceDetection::Hash::ResultsHash::getTrace() const {
	uint32_t i;
	stringstream trace;
	for (i = 0; i < results->count; i++) {
		trace << getTrace(i);
	}
	return trace.str();
}


int DeviceDetection::Hash::ResultsHash::getUserAgents() const {
	return results->count;
}

string DeviceDetection::Hash::ResultsHash::getUserAgent(
	int resultIndex) const {
	EXCEPTION_CREATE;

	// A negative index is not that of a result, so it is passed as an index
	// past every result, which still walks the graphs not yet walked.
	const char *userAgent = ResultsHashGetMatchedUserAgent(
		results,
		resultIndex < 0 ? UINT32_MAX : (uint32_t)resultIndex,
		exception);
	EXCEPTION_THROW;
	return userAgent == nullptr ? string() : string(userAgent);
}
