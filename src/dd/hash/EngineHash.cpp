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

#include <iostream>
#include <memory>
#include "EngineHash.hpp"
#include "fiftyone.h"

using namespace FiftyoneDegrees;
using namespace FiftyoneDegrees::DeviceDetection::Hash;

namespace {
	// Holds the names of the properties to index in the form the C layer
	// takes, which copies them before the init call returns.
	struct IndexedProperties {
		std::vector<std::string> names;
		std::vector<const char*> pointers;
		explicit IndexedProperties(
			const DeviceDetection::Hash::ConfigHash *config)
			: names(config->getIndexedProperties()) {
			for (const std::string &name : names) {
				pointers.push_back(name.c_str());
			}
		}
		const char** get() {
			return pointers.empty() ? nullptr : pointers.data();
		}
		int count() const {
			return (int)pointers.size();
		}
	};
}

EngineHash::EngineHash(
	const char *fileName,
	DeviceDetection::Hash::ConfigHash *config,
	Common::RequiredPropertiesConfig *properties)
	: EngineDeviceDetection(config, properties) {
	EXCEPTION_CREATE;
	IndexedProperties indexed(config);
	StatusCode status = HashInitManagerFromFileWithIndexedProperties(
		manager.get(),
		config->getConfig(),
		properties->getConfig(),
		fileName,
		indexed.get(),
		indexed.count(),
		exception);
	if (status != SUCCESS) {
		throw StatusCodeException(status, fileName);
		return;
	}
	EXCEPTION_THROW;
	init();
}

EngineHash::EngineHash(
	const string &fileName,
	DeviceDetection::Hash::ConfigHash *config,
	Common::RequiredPropertiesConfig *properties)
	: EngineHash(fileName.c_str(), config, properties) {
}

EngineHash::EngineHash(
	void *data,
	long length,
	DeviceDetection::Hash::ConfigHash *config,
	Common::RequiredPropertiesConfig *properties) 
	: DeviceDetection::EngineDeviceDetection(config, properties) {
	EXCEPTION_CREATE;

	// Copy the data and hand the responsibility for cleaning up to the C layer
	config->getConfig()->b.b.freeData = true;
	void *dataCopy = copyData(data, length);

	IndexedProperties indexed(config);
	StatusCode status = HashInitManagerFromMemoryWithIndexedProperties(
		manager.get(),
		config->getConfig(),
		properties->getConfig(),
		dataCopy,
		(size_t)length,
		indexed.get(),
		indexed.count(),
		exception);
	if (status != SUCCESS || EXCEPTION_FAILED) {
		// The memory stays with the caller until the data set is set up.
		Free(dataCopy);
	}
	if (status != SUCCESS) {
		throw StatusCodeException(status);
	}
	EXCEPTION_THROW;
	init();
}

EngineHash::EngineHash(
	unsigned char data[],
	long length,
	DeviceDetection::Hash::ConfigHash *config,
	Common::RequiredPropertiesConfig *properties)
	: EngineHash((void*)data, length, config, properties) {
}

void EngineHash::init() {
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	init(dataSet);
	DataSetHashRelease(dataSet);
}

void EngineHash::init(
	fiftyoneDegreesDataSetHash *dataSet) {
	EngineDeviceDetection::init(&dataSet->b);
	initMetaData();
	
	// Two new override properties available.
	addKey("query.51D_ProfileIds");
	addKey("cookie.51D_ProfileIds");
}

void* EngineHash::copyData(void *data, size_t length) const {
	void *dataCopy = (void*)Malloc(length);
	if (dataCopy == nullptr) {
		throw StatusCodeException(INSUFFICIENT_MEMORY);
	}
	memcpy(dataCopy, data, length);
	return dataCopy;
}

/**
 * @return the name of the data set used contained in the source file.
 */
string EngineHash::getProduct() const {
	stringstream stream;
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	appendString(stream, dataSet->strings, dataSet->header.nameOffset);
	DataSetHashRelease(dataSet);
	return stream.str();
}

/**
 * Returns the string that represents the type of data file when requesting an
 * updated file.
 */
string EngineHash::getType() const {
	return string("HashV41");
}

/**
 * @return the date that 51Degrees published the data file.
 */
Date EngineHash::getPublishedTime() const {
	DataSetHash*dataSet = DataSetHashGet(manager.get());
	Date date = Date(&dataSet->header.published);
	DataSetHashRelease(dataSet);
	return date;
}

/**
 * @return the date that 51Degrees will publish an updated data file.
 */
Date EngineHash::getUpdateAvailableTime() const {
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	Date date = Date(&dataSet->header.nextUpdate);
	DataSetHashRelease(dataSet);
	return date;
}

string EngineHash::getDataFilePath() const {
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	string path = string(dataSet->b.b.masterFileName);
	DataSetHashRelease(dataSet);
	return path;
}

string EngineHash::getDataFileTempPath() const {
	string path;
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	if (strcmp(
		dataSet->b.b.masterFileName,
		dataSet->b.b.fileName) == 0) {
		path = string("");
	}
	else {
		path = string(dataSet->b.b.fileName);
	}
	DataSetHashRelease(dataSet);
	return path;
}

void EngineHash::refreshData() const {
	EXCEPTION_CREATE;
	StatusCode status = HashReloadManagerFromOriginalFile(
		manager.get(),
		exception);
	if (status != SUCCESS) {
		throw StatusCodeException(status, getDataFilePath().c_str());
	}
	EXCEPTION_THROW;
}

void EngineHash::refreshData(const char *fileName) const {
	EXCEPTION_CREATE;
	StatusCode status = HashReloadManagerFromFile(
		manager.get(),
		fileName,
		exception);
	if (status != SUCCESS) {
		throw StatusCodeException(status, fileName);
	}
	EXCEPTION_THROW;
}

void EngineHash::refreshData(void *data, long length) const {
	EXCEPTION_CREATE;
	void *dataCopy = copyData(data, length);

	// The copy belongs to the engine rather than to the caller, so the data
	// set the reload creates has to take responsibility for freeing it. The
	// reload reads its configuration from the data set in use, which is
	// where the memory constructor's flag has to be set for a reload. Where
	// the engine was created from a file the flag is still false, and
	// without this the copy is never freed by anything.
	DataSetHash *dataSet = DataSetHashGet(manager.get());
	// The data set's copy of the configuration is const because it is only
	// written while the data set is initialised, which is also the only
	// place this flag is read, so the cast is safe here.
	((fiftyoneDegreesConfigHash*)&dataSet->config)->b.b.freeData = true;
	DataSetHashRelease(dataSet);

	StatusCode status = HashReloadManagerFromMemory(
		manager.get(),
		dataCopy,
		length,
		exception);
	if (status != SUCCESS) {
		throw StatusCodeException(status);
	}
	EXCEPTION_THROW;
}

void EngineHash::refreshData(
	unsigned char data[], 
	long length) const {
	refreshData((void*)data, length);
}

// Turns overrides on when creating results for evidence. The list of
// override values is sized by the data set, which knows how many properties
// evidence can override, so the number given here decides nothing else.
static const uint32_t overridesEnabled = 1;

// Owns a set of results until a ResultsHash takes them, so that they are freed
// on every way out of a process method.
typedef std::unique_ptr<
	fiftyoneDegreesResultsHash,
	decltype(&fiftyoneDegreesResultsHashFree)> OwnedResults;

// Creates a set of results for one call, and throws if there is no memory for
// them.
static OwnedResults createResults(
	fiftyoneDegreesResourceManager *manager,
	uint32_t overridesCapacity) {
	EXCEPTION_CREATE;
	OwnedResults results(
		ResultsHashCreate(manager, overridesCapacity),
		fiftyoneDegreesResultsHashFree);
	if (results == nullptr) {
		EXCEPTION_SET(FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY);
		EXCEPTION_THROW;
	}
	return results;
}

// Gives the results to the object returned to the caller, which frees them from
// then on.
static DeviceDetection::Hash::ResultsHash* giveResults(
	OwnedResults &results,
	const std::shared_ptr<fiftyoneDegreesResourceManager> &manager) {
	DeviceDetection::Hash::ResultsHash *given =
		new DeviceDetection::Hash::ResultsHash(results.get(), manager);
	results.release();
	return given;
}

DeviceDetection::Hash::ResultsHash* EngineHash::process(
	DeviceDetection::EvidenceDeviceDetection *evidence) const {
	return process(evidence, nullptr, -1);
}

DeviceDetection::Hash::ResultsHash* EngineHash::process(
	const char *userAgent) const {
	return process(userAgent, nullptr, -1);
}

// A set of results is created for each call rather than taken from a pool made
// at startup. A pool would avoid these allocations, but it would change when
// results are freed in every language, a reload would have to replace pooled
// results, and results reused without a reset could pass one request's override
// values to the next. That complexity is why a pool is not used, although it
// might be looked at in future.
DeviceDetection::Hash::ResultsHash* EngineHash::process(
	DeviceDetection::EvidenceDeviceDetection *evidence,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount) const {
	EXCEPTION_CREATE;

	OwnedResults results = createResults(manager.get(), overridesEnabled);
	ResultsHashFromEvidenceForProperties(
		results.get(),
		evidence == nullptr ? nullptr : evidence->get(),
		requiredPropertyIndexes,
		requiredPropertyIndexesCount,
		exception);
	EXCEPTION_THROW;

	return giveResults(results, manager);
}

DeviceDetection::Hash::ResultsHash* EngineHash::process(
	const char *userAgent,
	const int *requiredPropertyIndexes,
	int requiredPropertyIndexesCount) const {
	EXCEPTION_CREATE;

	// A User-Agent on its own carries no values to override, so the results
	// need no list of override values.
	OwnedResults results = createResults(manager.get(), 0);
	ResultsHashFromUserAgentForProperties(
		results.get(),
		userAgent,
		userAgent == nullptr ? 0 : strlen(userAgent),
		requiredPropertyIndexes,
		requiredPropertyIndexesCount,
		exception);
	EXCEPTION_THROW;
	return giveResults(results, manager);
}

Common::ResultsBase* EngineHash::processBase(
	Common::EvidenceBase *evidence) const {
	EXCEPTION_CREATE;
	OwnedResults results = createResults(manager.get(), overridesEnabled);
	ResultsHashFromEvidence(
		results.get(),
		evidence == nullptr ? nullptr : evidence->get(),
		exception);
	EXCEPTION_THROW;
	return giveResults(results, manager);
}

DeviceDetection::ResultsDeviceDetection* EngineHash::processDeviceDetection(
	DeviceDetection::EvidenceDeviceDetection *evidence) const {
	return process(evidence);
}

DeviceDetection::ResultsDeviceDetection* EngineHash::processDeviceDetection(
	const char *userAgent) const {
	return process(userAgent);
}

namespace {
	// The ids of the profiles the C layer passes. A failure to grow the list
	// is recorded rather than thrown through the C layer.
	struct ProfileIdsState {
		std::vector<uint32_t> ids;
		bool failed = false;
	};
}

static bool addProfileId(void *state, fiftyoneDegreesCollectionItem *item) {
	ProfileIdsState *profileIds = (ProfileIdsState*)state;
	try {
		profileIds->ids.push_back(
			((fiftyoneDegreesProfile*)item->data.ptr)->profileId);
	}
	catch (...) {
		profileIds->failed = true;
	}
	return true;
}

std::vector<uint32_t> EngineHash::getProfileIdsForPropertyValue(
	const std::string &propertyName,
	const std::string &valueName) const {
	EXCEPTION_CREATE;
	ProfileIdsState profileIds;
	HashIterateProfilesForPropertyAndValue(
		manager.get(),
		propertyName.c_str(),
		valueName.c_str(),
		&profileIds,
		addProfileId,
		exception);
	EXCEPTION_THROW;
	if (profileIds.failed) {
		throw StatusCodeException(INSUFFICIENT_MEMORY);
	}
	return profileIds.ids;
}

void EngineHash::initMetaData() {
	metaData = new MetaDataHash(manager);
}