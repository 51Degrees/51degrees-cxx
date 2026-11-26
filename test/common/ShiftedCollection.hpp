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

#ifndef FIFTYONE_DEGREES_SHIFTED_COLLECTION_HPP
#define FIFTYONE_DEGREES_SHIFTED_COLLECTION_HPP

#include "../../src/common/fiftyone.h"
#include <cstring>
#include <vector>

// Tests place data at every offset from 0 to this from an 8 byte boundary.
static constexpr size_t MAX_SHIFT_BYTES = 7;

/**
 * A memory collection whose 4 byte length header starts the given number of
 * bytes after an 8 byte boundary, as a collection in a data file can.
 */
class ShiftedCollection {
public:
	ShiftedCollection(
		const std::vector<byte> &records,
		uint32_t elementSize,
		size_t shift) {
		const uint32_t length = (uint32_t)records.size();
		buffer.resize((shift + sizeof(uint32_t) + length) /
			sizeof(uint64_t) + 1);
		start = (byte*)buffer.data() + shift;
		memcpy(start, &length, sizeof(length));
		if (length > 0) {
			memcpy(start + sizeof(uint32_t), records.data(), length);
		}
		fiftyoneDegreesMemoryReader reader;
		reader.startByte = start;
		reader.current = start;
		reader.length = (fiftyoneDegreesFileOffset)(sizeof(uint32_t) + length);
		reader.lastByte = start + reader.length;
		header = fiftyoneDegreesCollectionHeaderFromMemory(
			&reader,
			elementSize,
			false);
		collection = fiftyoneDegreesCollectionCreateFromMemory(
			&reader,
			header);
	}
	~ShiftedCollection() {
		collection->freeCollection(collection);
	}
	fiftyoneDegreesCollectionHeader header;
	fiftyoneDegreesCollection *collection;
	byte *start;
private:
	std::vector<uint64_t> buffer;
};

#endif
