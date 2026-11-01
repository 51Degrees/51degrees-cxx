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
 * @file IpiDoubleTextTests.cpp
 * @brief Checks the text of every value of the data file that holds numbers,
 * such as the polygons of Areas, is the same as when each number is worked out
 * one decimal place at a time.
 *
 * The text of a polygon is written again here from its well known binary, with
 * the numbers from DoubleText.hpp, and compared with the text from the library.
 */

#include <cstring>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../common/DoubleText.hpp"
#include "../../src/ipi/fiftyone.h"

using std::string;
using std::vector;

/** The decimal places of the text lookups give. */
static const uint8_t doubleTextLookupPlaces = 3;

/** The most decimal places tested for values that are a single number. */
static const uint8_t doubleTextMaxPlaces = 6;

/** Bytes for the text of a polygon to start with, enlarged when needed. */
static const size_t doubleTextBufferBytes = 1024 * 1024;

/**
 * Writes the text of well known binary as the library does, with the numbers
 * worked out one decimal place at a time. Only the byte order of the machine
 * is read, which is the order the data files use.
 */
class DoubleTextWkt {
public:
	DoubleTextWkt(
		const unsigned char* wkb,
		const bool isReduced,
		const uint8_t decimalPlaces) :
		next(wkb), reduced(isReduced), places(decimalPlaces) {}

	/** Writes the geometry, returning false if it cannot be read. */
	bool write() {
		return geometry(false);
	}

	string text;
	uint32_t coordinates = 0;

private:
	const unsigned char* next;
	const bool reduced;
	const uint8_t places;
	bool separated = true;
	uint32_t dimensions = 2;

	template <typename T> T read() {
		T value;
		memcpy(&value, next, sizeof(T));
		next += sizeof(T);
		return value;
	}

	uint32_t readType() {
		return reduced ? read<uint8_t>() : read<uint32_t>();
	}

	uint32_t readCount() {
		return reduced ? read<uint16_t>() : read<uint32_t>();
	}

	double readCoordinate(const uint32_t index) {
		if (reduced == false) {
			return read<double>();
		}
		const double angle = index % 2 == 0 ? 180.0 : 90.0;
		return (read<int16_t>() * angle) / INT16_MAX;
	}

	void empty() {
		if (separated == false) {
			text += ' ';
		}
		text += "EMPTY";
		separated = false;
	}

	bool point() {
		for (uint32_t i = 0; i < dimensions; i++) {
			if (i > 0) {
				text += ' ';
			}
			text += doubleTextByPlace(readCoordinate(i), places);
			coordinates++;
			separated = false;
		}
		return true;
	}

	bool ring() {
		return loop(&DoubleTextWkt::point);
	}

	bool known() {
		return geometry(true);
	}

	bool unknown() {
		return geometry(false);
	}

	bool parentheses(bool (DoubleTextWkt::*item)(), const uint32_t count) {
		text += '(';
		separated = true;
		for (uint32_t i = 0; i < count; i++) {
			if (i > 0) {
				text += ',';
				separated = true;
			}
			if ((this->*item)() == false) {
				return false;
			}
		}
		text += ')';
		separated = true;
		return true;
	}

	bool loop(bool (DoubleTextWkt::*item)()) {
		const uint32_t count = readCount();
		if (count == 0) {
			empty();
			return true;
		}
		return parentheses(item, count);
	}

	bool geometry(const bool typeIsKnown) {
		static const uint32_t dimensionsOfMode[] = { 2, 3, 3, 4 };
		static const char* const tagOfMode[] = { nullptr, "Z", "M", "ZM" };
		if (read<uint8_t>() != machineByteOrder()) {
			return false;
		}
		const uint32_t type = readType();
		const uint32_t mode = type / 1000;
		const uint32_t code = type % 1000;
		if (mode > 3) {
			return false;
		}
		dimensions = dimensionsOfMode[mode];
		const char* name = nullptr;
		bool counted = true;
		bool (DoubleTextWkt::*item)() = nullptr;
		switch (code) {
		case 0: name = "GEOMETRY"; counted = false; break;
		case 1: name = "POINT"; counted = false; item = &DoubleTextWkt::point; break;
		case 2: name = "LINESTRING"; item = &DoubleTextWkt::point; break;
		case 3: name = "POLYGON"; item = &DoubleTextWkt::ring; break;
		case 4: name = "MULTIPOINT"; item = &DoubleTextWkt::known; break;
		case 5: name = "MULTILINESTRING"; item = &DoubleTextWkt::known; break;
		case 6: name = "MULTIPOLYGON"; item = &DoubleTextWkt::known; break;
		case 7: name = "GEOMETRYCOLLECTION"; item = &DoubleTextWkt::unknown; break;
		case 15: name = "POLYHEDRALSURFACE"; item = &DoubleTextWkt::known; break;
		case 16: name = "TIN"; item = &DoubleTextWkt::known; break;
		case 17: name = "TRIANGLE"; item = &DoubleTextWkt::ring; break;
		default: return false;
		}
		if (typeIsKnown == false) {
			text += name;
			if (tagOfMode[mode] != nullptr) {
				text += ' ';
				text += tagOfMode[mode];
			}
			separated = false;
		}
		if (code == 0) {
			text += '(';
			separated = true;
			empty();
			text += ')';
			separated = true;
			return true;
		}
		return counted ? loop(item) : parentheses(item, 1);
	}

	static uint8_t machineByteOrder() {
		const uint32_t one = 1;
		uint8_t first;
		memcpy(&first, &one, 1);
		return first;
	}
};

class IpiDoubleTextTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		for (int i = 0; i < _IpiFileNamesLength && dataFilePath.empty(); i++) {
			dataFilePath = GetFilePath(_dataFolderName, _IpiFileNames[i]);
		}
		if (dataFilePath.empty()) {
			GTEST_SKIP() << "No data file was found.";
		}
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesIpiInitManagerFromFile(
				&manager,
				&config,
				&properties,
				dataFilePath.c_str(),
				exception);
		ASSERT_EQ(FIFTYONE_DEGREES_STATUS_SUCCESS, status);
		EXCEPTION_THROW;
		loaded = true;
	}

	void TearDown() override {
		if (loaded) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		string().swap(dataFilePath);
		Base::TearDown();
	}

protected:
	string dataFilePath;
	fiftyoneDegreesResourceManager manager;
	bool loaded = false;

	/**
	 * Calls the method with each value of each property stored as one of the
	 * types, returning the number of values.
	 */
	template <typename Method>
	uint32_t forEachValue(
		const fiftyoneDegreesPropertyValueType* types,
		const size_t typesCount,
		Method method) {
		uint32_t values = 0;
		fiftyoneDegreesDataSetIpi* dataSet =
			fiftyoneDegreesDataSetIpiGet(&manager);
		const uint32_t propertiesCount =
			fiftyoneDegreesCollectionGetCount(dataSet->properties);
		for (uint32_t p = 0; p < propertiesCount; p++) {
			EXCEPTION_CREATE;
			const fiftyoneDegreesPropertyValueType type =
				fiftyoneDegreesPropertyGetStoredTypeByIndex(
					dataSet->propertyTypes,
					p,
					exception);
			EXCEPTION_THROW;
			bool wanted = false;
			for (size_t t = 0; t < typesCount; t++) {
				wanted = wanted || types[t] == type;
			}
			if (wanted == false) {
				continue;
			}
			fiftyoneDegreesCollectionItem propertyItem;
			fiftyoneDegreesDataReset(&propertyItem.data);
			const fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
				dataSet->properties,
				p,
				&propertyItem,
				exception);
			EXCEPTION_THROW;
			const uint32_t first = property->firstValueIndex;
			const uint32_t last = property->lastValueIndex;
			FIFTYONE_DEGREES_COLLECTION_RELEASE(
				dataSet->properties,
				&propertyItem);
			for (uint32_t v = first; v <= last && first <= last; v++) {
				fiftyoneDegreesCollectionItem valueItem;
				fiftyoneDegreesDataReset(&valueItem.data);
				const fiftyoneDegreesValue* value = fiftyoneDegreesValueGet(
					dataSet->values,
					v,
					&valueItem,
					exception);
				EXCEPTION_THROW;
				const uint32_t offset = (uint32_t)value->nameOffset;
				FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->values, &valueItem);
				fiftyoneDegreesCollectionItem stringItem;
				fiftyoneDegreesDataReset(&stringItem.data);
				const fiftyoneDegreesStoredBinaryValue* stored =
					fiftyoneDegreesStoredBinaryValueGet(
						dataSet->strings,
						offset,
						type,
						&stringItem,
						exception);
				EXCEPTION_THROW;
				method(stored, type, v);
				FIFTYONE_DEGREES_COLLECTION_RELEASE(
					dataSet->strings,
					&stringItem);
				values++;
			}
		}
		fiftyoneDegreesDataSetIpiRelease(dataSet);
		return values;
	}
};

TEST_F(IpiDoubleTextTests, EveryGeometryIsUnchanged) {
	const fiftyoneDegreesPropertyValueType types[] = {
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB_R };
	vector<char> buffer(doubleTextBufferBytes);
	uint64_t coordinates = 0;
	uint32_t failures = 0;
	const uint32_t values = forEachValue(
		types,
		sizeof(types) / sizeof(types[0]),
		[&](const fiftyoneDegreesStoredBinaryValue* stored,
			fiftyoneDegreesPropertyValueType type,
			uint32_t index) {
			const bool reduced =
				type == FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_WKB_R;
			const unsigned char* wkb = &stored->byteArrayValue.firstByte;
			DoubleTextWkt expected(wkb, reduced, doubleTextLookupPlaces);
			if (expected.write() == false) {
				if (failures++ < 10) {
					ADD_FAILURE() << "Value " << index << " could not be read.";
				}
				return;
			}
			coordinates += expected.coordinates;
			// Writes the text into the buffer, returning the bytes it needs
			// if the buffer is too small.
			const auto writeText = [&]() {
				EXCEPTION_CREATE;
				fiftyoneDegreesStringBuilder builder = {
					buffer.data(),
					buffer.size() };
				fiftyoneDegreesStringBuilderInit(&builder);
				fiftyoneDegreesWriteWkbAsWktToStringBuilder(
					wkb,
					reduced ?
						FIFTYONE_DEGREES_WKBToT_REDUCTION_SHORT :
						FIFTYONE_DEGREES_WKBToT_REDUCTION_NONE,
					doubleTextLookupPlaces,
					&builder,
					exception);
				EXCEPTION_THROW;
				fiftyoneDegreesStringBuilderComplete(&builder);
				return builder.full ? builder.added : (size_t)0;
			};
			const size_t needed = writeText();
			if (needed > 0) {
				buffer.resize(needed);
				writeText();
			}
			if (expected.text != buffer.data()) {
				if (failures++ < 10) {
					ADD_FAILURE() << "Value " << index << " gave '" <<
						string(buffer.data()).substr(0, 200) <<
						"' rather than '" << expected.text.substr(0, 200) <<
						"'";
				}
			}
		});
	if (values == 0) {
		GTEST_SKIP() << "The data file has no geometries.";
	}
	EXPECT_EQ(0u, failures);
	std::cout << "Compared " << values << " geometries holding " <<
		coordinates << " coordinates.\n";
}

TEST_F(IpiDoubleTextTests, EveryNumberIsUnchanged) {
	const fiftyoneDegreesPropertyValueType types[] = {
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_AZIMUTH,
		FIFTYONE_DEGREES_PROPERTY_VALUE_TYPE_DECLINATION,
		FIFTYONE_DEGREES_PROPERTY_VALUE_SINGLE_PRECISION_FLOAT };
	char buffer[64];
	uint32_t failures = 0;
	const uint32_t values = forEachValue(
		types,
		sizeof(types) / sizeof(types[0]),
		[&](const fiftyoneDegreesStoredBinaryValue* stored,
			fiftyoneDegreesPropertyValueType type,
			uint32_t index) {
			const double number =
				fiftyoneDegreesStoredBinaryValueToDoubleOrDefault(
					stored,
					type,
					0);
			for (uint8_t places = 0; places <= doubleTextMaxPlaces; places++) {
				EXCEPTION_CREATE;
				fiftyoneDegreesStringBuilder builder = {
					buffer,
					sizeof(buffer) };
				fiftyoneDegreesStringBuilderInit(&builder);
				fiftyoneDegreesStringBuilderAddStringValue(
					&builder,
					stored,
					type,
					places,
					exception);
				EXCEPTION_THROW;
				fiftyoneDegreesStringBuilderComplete(&builder);
				if (doubleTextByPlace(number, places) != buffer &&
					failures++ < 10) {
					ADD_FAILURE() << "Value " << index << " with " <<
						(int)places << " places gave '" << buffer << "'";
				}
			}
		});
	if (values == 0) {
		GTEST_SKIP() << "The data file has no values that are numbers.";
	}
	EXPECT_EQ(0u, failures);
	std::cout << "Compared " << values << " numbers.\n";
}
