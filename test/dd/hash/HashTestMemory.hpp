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

#ifndef FIFTYONE_DEGREES_HASH_TEST_MEMORY_HPP
#define FIFTYONE_DEGREES_HASH_TEST_MEMORY_HPP

/**
 * @file HashTestMemory.hpp
 * @brief Builds memory to offer the engine in place of a data file.
 *
 * Memory that gets past the version has every collection empty and straight
 * after the header, then a test changes one thing, so the failure the test
 * looks for is the first one reached.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
#include "../../../src/dd/hash/hash.h"

typedef fiftyoneDegreesDataSetHashHeader TestHeader;
typedef std::vector<unsigned char> TestMemory;

inline void setInt32(TestMemory &memory, size_t offset, int32_t value) {
	memcpy(memory.data() + offset, &value, sizeof(value));
}

inline void setCollection(
	TestMemory &memory,
	size_t offset,
	size_t start,
	uint32_t length,
	uint32_t count = 0) {
	fiftyoneDegreesCollectionHeader header;
	header.startPosition = (fiftyoneDegreesFileOffsetUnsigned)start;
	header.length = length;
	header.count = count;
	memcpy(memory.data() + offset, &header, sizeof(header));
}

/**
 * Gets bytes that mean nothing to the engine, the same on every run.
 */
inline TestMemory garbage(size_t length) {
	TestMemory memory(length);
	std::minstd_rand generator(1);
	for (unsigned char &value : memory) {
		value = (unsigned char)generator();
	}
	return memory;
}

inline void setVersion(TestMemory &memory) {
	setInt32(memory, offsetof(TestHeader, versionMajor), 4);
	setInt32(memory, offsetof(TestHeader, versionMinor), 1);
}

inline TestMemory emptyDataSet() {
	TestMemory memory(sizeof(TestHeader), 0);
	setVersion(memory);
	const size_t collections[] = {
		offsetof(TestHeader, strings),
		offsetof(TestHeader, components),
		offsetof(TestHeader, maps),
		offsetof(TestHeader, properties),
		offsetof(TestHeader, values),
		offsetof(TestHeader, profiles),
		offsetof(TestHeader, rootNodes),
		offsetof(TestHeader, nodes),
		offsetof(TestHeader, profileOffsets)
	};
	for (const size_t collection : collections) {
		setCollection(memory, collection, sizeof(TestHeader), 0);
	}
	return memory;
}

/**
 * Gets a data set whose only content is components with no key values. It
 * loads if the only required property is the device id, which needs no stored
 * property, so a test can reach the end of the init method with no data file.
 */
inline TestMemory dataSetWithComponents(uint32_t count) {
	const uint32_t size = (uint32_t)(sizeof(fiftyoneDegreesComponent) -
		sizeof(fiftyoneDegreesComponentKeyValuePair));
	TestMemory memory = emptyDataSet();
	memory.resize(sizeof(TestHeader) + (size_t)size * count, 0);
	setCollection(
		memory,
		offsetof(TestHeader, components),
		sizeof(TestHeader),
		size * count,
		count);
	for (uint32_t i = 0; i < count; i++) {
		memory[sizeof(TestHeader) + (size_t)size * i] = (unsigned char)i;
	}
	const size_t afterComponents[] = {
		offsetof(TestHeader, maps),
		offsetof(TestHeader, properties),
		offsetof(TestHeader, values),
		offsetof(TestHeader, profiles),
		offsetof(TestHeader, rootNodes),
		offsetof(TestHeader, nodes),
		offsetof(TestHeader, profileOffsets)
	};
	for (const size_t collection : afterComponents) {
		setCollection(memory, collection, memory.size(), 0);
	}
	return memory;
}

#endif
