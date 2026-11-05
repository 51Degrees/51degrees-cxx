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
 * @file IpiValueAsDoubleTests.cpp
 * @brief Tests that a latitude or longitude read as a double is the value the
 * data file stores, and not its text read back.
 *
 * The value types specification converts a stored declination to a latitude
 * as 90.0 * (stored / 32767.0), and a stored azimuth to a longitude as
 * 180.0 * (stored / 32767.0), with no rounding. The stored value is read here
 * through the C API and converted with those expressions, then compared with
 * each double getter of the C++ results. The text of a value keeps its
 * fiftyoneDegreesDefaultWktDecimalPlaces decimal places.
 */

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/constantsIpi.h"
#include "../../src/ipi/fiftyone.h"

using std::string;
using std::unique_ptr;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

/** The properties read, in the order of their required property indexes. */
static const char valueAsDoubleProperties[] = "Latitude,Longitude";
static const char* const valueAsDoubleNames[] = { "Latitude", "Longitude" };
static const int valueAsDoubleNamesCount = 2;

/** Addresses, IPv4 and IPv6, that the data file locates. */
static const char* const valueAsDoubleAddresses[] = {
	"1.1.1.1",
	"::ffff:1.1.1.1",
	"8.8.8.8",
	"51.51.51.51",
	"2a00:1450:4009:81f::200e" };

/** The scale of a stored declination and a stored azimuth. */
static const double valueAsDoubleDeclinationDegrees = 90.0;
static const double valueAsDoubleAzimuthDegrees = 180.0;
static const double valueAsDoubleStoredMax = 32767.0;

class IpiValueAsDoubleTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0; i < _IpiFileNamesLength && dataFilePath.empty(); i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No data file was found.";
		}
		fiftyoneDegreesConfigIpi cConfig = fiftyoneDegreesIpiLowMemoryConfig;
		fiftyoneDegreesPropertiesRequired cProperties =
			fiftyoneDegreesPropertiesDefault;
		cProperties.string = valueAsDoubleProperties;
		EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&cConfig,
				&cProperties,
				dataFilePath.c_str(),
				exception);
		if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
			GTEST_SKIP() << "The data file has no " << valueAsDoubleProperties;
		}
		loaded = true;
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		const uint32_t count = dataSet->b.b.available->count;
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		if (count != (uint32_t)valueAsDoubleNamesCount) {
			GTEST_SKIP() << "The data file has only " << count << " of " <<
				valueAsDoubleProperties;
		}
		Ipi::ConfigIpi config;
		config.setLowMemory();
		Common::RequiredPropertiesConfig properties(valueAsDoubleProperties);
		engine.reset(new Ipi::EngineIpi(dataFilePath, &config, &properties));
	}

	void TearDown() override {
		engine.reset();
		if (loaded) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		string().swap(dataFilePath);
		Base::TearDown();
	}

protected:
	string dataFilePath;
	fiftyoneDegreesResourceManager manager = {};
	bool loaded = false;
	unique_ptr<Ipi::EngineIpi> engine;

	/**
	 * Gets the value the data file stores for the address, converted as the
	 * value types specification says. Returns false if the property does not
	 * have one value, stored as a declination or an azimuth.
	 */
	bool storedValue(
		const char* ipAddress,
		const int requiredPropertyIndex,
		double& value) {
		bool found = false;
		EXCEPTION_CREATE;
		fiftyoneDegreesResultsIpi* results =
			fiftyoneDegreesResultsIpiCreate(&manager);
		fiftyoneDegreesResultsIpiFromIpAddressString(
			results,
			ipAddress,
			strlen(ipAddress),
			exception);
		const fiftyoneDegreesProfilePercentage* values = nullptr;
		if (EXCEPTION_OKAY) {
			values = fiftyoneDegreesResultsIpiGetValues(
				results,
				requiredPropertyIndex,
				exception);
		}
		if (EXCEPTION_OKAY && values != nullptr && results->values.count == 1) {
			const fiftyoneDegreesDataSetIpi* dataSet =
				(const fiftyoneDegreesDataSetIpi*)results->b.dataSet;
			const int propertyIndex =
				fiftyoneDegreesPropertiesGetPropertyIndexFromRequiredIndex(
					dataSet->b.b.available,
					requiredPropertyIndex);
			const fiftyoneDegreesPropertyValueType type =
				fiftyoneDegreesPropertyGetStoredTypeByIndex(
					dataSet->propertyTypes,
					(uint32_t)propertyIndex,
					exception);
			const int16_t stored = ((const fiftyoneDegreesStoredBinaryValue*)
				values[0].item.data.ptr)->shortValue;
			if (EXCEPTION_OKAY &&
				type == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION) {
				value = valueAsDoubleDeclinationDegrees *
					((double)stored / valueAsDoubleStoredMax);
				found = true;
			}
			else if (EXCEPTION_OKAY &&
				type == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH) {
				value = valueAsDoubleAzimuthDegrees *
					((double)stored / valueAsDoubleStoredMax);
				found = true;
			}
		}
		fiftyoneDegreesResultsIpiFree(results);
		EXCEPTION_THROW;
		return found;
	}
};

/** Expects a double getter to give the value, read in the way described. */
static void expectValue(
	const double expected,
	const Common::Value<double>& value,
	const string& read) {
	ASSERT_TRUE(value.hasValue()) << read << " has no value, reason " <<
		(int)value.getNoValueReason();
	EXPECT_DOUBLE_EQ(expected, value.getValue()) << read;
}

/** The decimal places of the text of a value, without its weight. */
static size_t decimalPlacesOf(const string& text) {
	const string number = text.substr(0, text.find(':'));
	const size_t point = number.find('.');
	return point == string::npos ? 0 : number.size() - point - 1;
}

TEST_F(IpiValueAsDoubleTests, DoublesAreTheStoredValue) {
	uint32_t compared = 0;
	uint32_t morePlaces = 0;
	for (const char* address : valueAsDoubleAddresses) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(address));
		for (int i = 0; i < valueAsDoubleNamesCount; i++) {
			double expected = 0;
			if (storedValue(address, i, expected) == false) {
				continue;
			}
			compared++;
			const double atTextPlaces = std::pow(
				10.0,
				fiftyoneDegreesDefaultWktDecimalPlaces);
			if (std::round(expected * atTextPlaces) / atTextPlaces != expected) {
				morePlaces++;
			}
			const char* name = valueAsDoubleNames[i];
			const string read = string(name) + " of " + address;
			expectValue(
				expected,
				results->getValueAsDouble(name),
				read + " by name");
			expectValue(
				expected,
				results->getValueAsDouble(i),
				read + " by index");
			expectValue(
				expected,
				results->getValueAsStrictDouble(name),
				read + " strictly by name");
			expectValue(
				expected,
				results->getValueAsStrictDouble(i),
				read + " strictly by index");
		}
	}
	ASSERT_GT(compared, 0u) << "No address has one stored value for each of " <<
		valueAsDoubleProperties;
	EXPECT_GT(morePlaces, 0u) << "Every value fits the decimal places of its " <<
		"text, so this cannot tell the stored value from the text.";
}

TEST_F(IpiValueAsDoubleTests, TextKeepsItsDecimalPlaces) {
	uint32_t compared = 0;
	for (const char* address : valueAsDoubleAddresses) {
		unique_ptr<Ipi::ResultsIpi> results(engine->process(address));
		for (int i = 0; i < valueAsDoubleNamesCount; i++) {
			double expected = 0;
			if (storedValue(address, i, expected) == false) {
				continue;
			}
			compared++;
			const Common::Value<string> text =
				results->getValueAsString(valueAsDoubleNames[i]);
			ASSERT_TRUE(text.hasValue()) << valueAsDoubleNames[i] << " of " <<
				address;
			EXPECT_LE(
				decimalPlacesOf(text.getValue()),
				(size_t)fiftyoneDegreesDefaultWktDecimalPlaces) <<
				valueAsDoubleNames[i] << " of " << address << " is '" <<
				text.getValue() << "'";
		}
	}
	ASSERT_GT(compared, 0u) << "No address has one stored value for each of " <<
		valueAsDoubleProperties;
}
