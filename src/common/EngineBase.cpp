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

#include "EngineBase.hpp"

#include "fiftyone.h"
#include "string_pp.hpp"

using namespace FiftyoneDegrees::Common;

EngineBase::EngineBase(
	ConfigBase *config,
	RequiredPropertiesConfig *requiredProperties) {
	defaultDataKey = "device";
	updateUrl = string("");
	licenceKey = string("");
	manager = make_shared<fiftyoneDegreesResourceManager>();
	manager->active = nullptr;
	this->requiredProperties = requiredProperties;
	this->config = config;
	metaData = nullptr;
	keyLists.emplace_back(new vector<string>());
}

EngineBase::~EngineBase() {
	
	// Free the meta data if set.
	delete metaData;
	
	// Release the resource used by the engine.
	if (manager->active != nullptr) {
		ResourceManagerFree(manager.get());
	}
}

vector<string> EngineBase::readKeys(
	const std::function<size_t(char*, size_t, const char*)> &write) {
	// No key holds a comma, as no HTTP header name holds one and property
	// names are given as a comma separated list.
	static const char separator[] = ",";
	vector<char> buffer(write(nullptr, 0, separator) + 1);
	write(buffer.data(), buffer.size(), separator);
	const string text(buffer.data());
	vector<string> keys;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find(separator[0], start);
		if (end == string::npos) {
			end = text.size();
		}
		keys.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	return keys;
}

void EngineBase::setLicenseKey(const string &licenseKey) {
	this->licenceKey.assign(licenseKey);
}

void EngineBase::setDataUpdateUrl(const string &newUpdateUrl) {
	this->updateUrl.assign(newUpdateUrl);
}

string EngineBase::getDataUpdateUrl() const {
	stringstream stream;
	if (updateUrl.empty() == false) {
		stream << updateUrl;
	}
	else if (licenceKey.empty() == false) {
		stream << "https://distributor.51degrees.com/api/v2/download"
			<< "?LicenceKeys=" << licenceKey.c_str()
			<< "&Type=" << getType()
			<< "&Product=" << getProduct();
	}
	return stream.str();
}

MetaData* EngineBase::getMetaData() const {
	return metaData;
}

bool EngineBase::getAutomaticUpdatesEnabled() const {
	return getDataUpdateUrl().empty() == false;
}

void EngineBase::setKeys(vector<string> &&newKeys) const {
	std::lock_guard<std::mutex> lock(keysMutex);
	if (*keyLists.back() == newKeys) {
		return;
	}
	keyLists.emplace_back(new vector<string>(std::move(newKeys)));
}

const vector<string>* EngineBase::getKeys() const {
	std::lock_guard<std::mutex> lock(keysMutex);
	return keyLists.back().get();
}

vector<string> EngineBase::getRequiredProperties() const {
	vector<string> names;
	DataSetBase *dataSet = DataSetGet(manager.get());
	if (dataSet != nullptr) {
		PropertiesAvailable *available = dataSet->available;
		if (available != nullptr) {
			const uint32_t count = PropertiesGetRequiredPropertyCount(
				available);
			for (uint32_t i = 0; i < count; i++) {
				const char *name = STRING(PropertiesGetNameFromRequiredIndex(
					available,
					(int)i));
				names.push_back(name == nullptr ? string() : string(name));
			}
		}
		DataSetRelease(dataSet);
	}
	return names;
}

bool EngineBase::getIsThreadSafe() const {
	return ThreadingGetIsThreadSafe();
}

void EngineBase::appendValue(
	stringstream &stream,
	fiftyoneDegreesCollection *strings,
	uint32_t offset,
	PropertyValueType storedValueType) const {
	EXCEPTION_CREATE;
	Item item;
	DataReset(&item.data);
	const StoredBinaryValue * const binaryValue = StoredBinaryValueGet(
		strings,
		offset,
		storedValueType,
		&item,
		exception);
	if (binaryValue != nullptr && EXCEPTION_OKAY) {
		writeStoredBinaryValueToStringStream(
			binaryValue,
			storedValueType,
			stream,
			MAX_DOUBLE_DECIMAL_PLACES,
			exception);
		COLLECTION_RELEASE(strings, &item);
	}
	EXCEPTION_THROW;
}

void EngineBase::appendString(
	stringstream &stream,
	fiftyoneDegreesCollection *strings,
	uint32_t offset) const {
	appendValue(
		stream,
		strings,
		offset,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_STRING); // legacy contract
}