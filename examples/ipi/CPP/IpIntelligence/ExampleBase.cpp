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

#include "ExampleBase.hpp"
#include "../../Base/ExampleBase.h"

using std::cout;
using namespace FiftyoneDegrees;
using namespace FiftyoneDegrees::Examples::IpIntelligence;

const char *ExampleBase::ipv4Address = "116.154.188.222";

const char *ExampleBase::ipv6Address = "2001:db8::";

const char *ExampleBase::needsEnterprise =
	FIFTYONE_DEGREES_EXAMPLE_NEEDS_ENTERPRISE;

ExampleBase::ExampleBase(::byte *data, FileOffset length, const std::shared_ptr<FiftyoneDegrees::IpIntelligence::ConfigIpi> &config) {
	this->config = config;
	
	// Set the properties to be returned for each Ip Address. AsnName is for
	// the free ASN data file, which has none of the others.
	string propertiesString = "IpRangeStart,IpRangeEnd,RegisteredCountry,AccuracyRadiusMin,Longitude,Latitude,AsnName";
	properties = std::make_unique<RequiredPropertiesConfig>(propertiesString);
	
	// Initialise the engine for IP intelligence.
	engine = std::make_unique<EngineIpi>(data, length, config.get(), properties.get());
}

ExampleBase::ExampleBase(const string& dataFilePath, const std::shared_ptr<FiftyoneDegrees::IpIntelligence::ConfigIpi> &config) {
	this->config = config;
	
	// Set the properties to be returned for each Ip Address. AsnName is for
	// the free ASN data file, which has none of the others.
	string propertiesString = "IpRangeStart,IpRangeEnd,AccuracyRadiusMin,RegisteredCountry,RegisteredName,Longitude,Latitude,Areas,AsnName";
	properties = std::make_unique<RequiredPropertiesConfig>(propertiesString);

	// Initialise the engine for ip intelligence.
	engine = std::make_unique<EngineIpi>(dataFilePath, config.get(), properties.get());
}

ExampleBase::ExampleBase(const string& dataFilePath)
    : ExampleBase(dataFilePath, std::make_unique<FiftyoneDegrees::IpIntelligence::ConfigIpi>()) {}

ExampleBase::~ExampleBase() = default;

void ExampleBase::reportStatus(fiftyoneDegreesStatusCode status,
                               const char *fileName) {
	const char *message = fiftyoneDegreesStatusGetMessage(status, fileName);
	cout << message << "\n";
	fiftyoneDegreesFree((void *)message);
}

fiftyoneDegreesStatusCode ExampleBase::getFilePath(
	const char *dataFolderName,
	const char *fileName,
	char *destination,
	size_t size) {
	return fiftyoneDegreesExampleGetFilePath(
		dataFolderName,
		fileName,
		destination,
		size);
}

const char *ExampleBase::getDataFilePathFromEnvironment() {
	return fiftyoneDegreesExampleGetDataFilePathFromEnvironment();
}

void ExampleBase::findOutMore() {
	fiftyoneDegreesExampleFindOutMore(stdout);
}

bool ExampleBase::hasProperty(const char *name) const {
	auto const fileProperties =
		std::unique_ptr<Collection<string, PropertyMetaData>>(
			engine->getMetaData()->getProperties());
	auto const property = std::unique_ptr<PropertyMetaData>(
		fileProperties->getByKey(name));
	return property != nullptr;
}

void ExampleBase::offerEnterprise(const char *contactUs) const {
	// Only the Enterprise data file has the range properties, so a check for
	// IpRangeStart tells the free data files, Lite and ASN, apart from it.
	if (!hasProperty("IpRangeStart")) {
		cout << "\nThis example is using a free data file. The Enterprise "
			"data file adds the location, range and connection properties. "
			"Contact us for a copy at " << contactUs << "\n";
	}
}

unsigned long ExampleBase::generateHash(unsigned char *value) {
	unsigned long hashCode = 5381;
	int i = *value++;
	while (i != 0) {
		hashCode = ((hashCode << 5) + hashCode) + i;
		i = *value++;
	}
	return hashCode;
}

unsigned long ExampleBase::getHashCode(FiftyoneDegrees::IpIntelligence::ResultsIpi *results) {
	unsigned long hashCode = 0;
	string valueName;

	for (const string &name : results->getProperties()) {
		Common::Value<string> nextValue = results->getValueAsString(name);
		if (nextValue.hasValue()) {
			valueName = nextValue.getValue();
			hashCode ^= generateHash((unsigned char *)(valueName.c_str()));
		}
	}
	return hashCode;
}

void ExampleBase::processIpAddress(const char *ipAddress, void *state) {
	ThreadState *thread = (ThreadState *)state;
	
	auto const results = std::unique_ptr<FiftyoneDegrees::IpIntelligence::ResultsIpi>(thread->engine->process(ipAddress));
	
	thread->hashCode ^= getHashCode(results.get());
}

void ExampleBase::SharedState::processIpAddressesSingle() {
	char ipAddress[500] = "";
	ThreadState thread(engine);
	fiftyoneDegreesEvidenceFileIterate(ipAddressFilePath.c_str(), ipAddress,
								sizeof(ipAddress), &thread, processIpAddress);
	printf("Finished with hash code '%i'\r\n", thread.hashCode);
}

void ExampleBase::SharedState::processIpAddressesMulti(void *state) {
	SharedState *shared = (SharedState *)state;
	shared->processIpAddressesSingle();
	FIFTYONE_DEGREES_INTERLOCK_INC(&shared->threadsFinished);
}

void ExampleBase::SharedState::startThreads() {
	int i;
	for (i = 0; i < THREAD_COUNT; i++) {
		threads[i] = thread(processIpAddressesMulti, this);
	}
}

void ExampleBase::SharedState::joinThreads() {
	int i;
	for (i = 0; i < THREAD_COUNT; i++) {
		threads[i].join();
	}
}

ExampleBase::SharedState::SharedState(EngineIpi * const engine,
                                      const string &ipAddressFilePath) {
	this->engine = engine;
	this->threadsFinished = 0;
	this->ipAddressFilePath = ipAddressFilePath;
}

ExampleBase::ThreadState::ThreadState(
    EngineIpi * const engine) {
	this->engine = engine;
	this->hashCode = 0;
}
