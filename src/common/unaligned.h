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

#ifndef FIFTYONE_DEGREES_UNALIGNED_H_INCLUDED
#define FIFTYONE_DEGREES_UNALIGNED_H_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

/**
 * @ingroup FiftyOneDegreesCommon
 * @defgroup FiftyOneDegreesUnaligned Unaligned
 *
 * Read and write numbers that can start at any byte.
 *
 * The data file holds its records packed, so a number within it can start at
 * any byte. Reading one through a pointer to its type is undefined when the
 * address is not aligned to the type, and with vector instructions it can
 * fault. Each function here copies the bytes of one number between the
 * address given and a local of the number's type. The copy has a fixed size,
 * so compilers make it a single load or store where the processor allows one.
 *
 * @{
 */

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#define FIFTYONE_DEGREES_UNALIGNED_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define FIFTYONE_DEGREES_UNALIGNED_INLINE static __inline__
#else
#define FIFTYONE_DEGREES_UNALIGNED_INLINE static inline
#endif

/**
 * Reads a signed 16 bit integer that starts at any byte.
 * @param source address of the integer's first byte
 * @return the integer
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE int16_t fiftyoneDegreesUnalignedReadInt16(
	const void * const source) {
	int16_t value;
	memcpy(&value, source, sizeof(value));
	return value;
}

/**
 * Reads an unsigned 16 bit integer that starts at any byte.
 * @param source address of the integer's first byte
 * @return the integer
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE uint16_t fiftyoneDegreesUnalignedReadUint16(
	const void * const source) {
	uint16_t value;
	memcpy(&value, source, sizeof(value));
	return value;
}

/**
 * Reads a signed 32 bit integer that starts at any byte.
 * @param source address of the integer's first byte
 * @return the integer
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE int32_t fiftyoneDegreesUnalignedReadInt32(
	const void * const source) {
	int32_t value;
	memcpy(&value, source, sizeof(value));
	return value;
}

/**
 * Reads an unsigned 32 bit integer that starts at any byte.
 * @param source address of the integer's first byte
 * @return the integer
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE uint32_t fiftyoneDegreesUnalignedReadUint32(
	const void * const source) {
	uint32_t value;
	memcpy(&value, source, sizeof(value));
	return value;
}

/**
 * Reads a double precision floating point number that starts at any byte.
 * @param source address of the number's first byte
 * @return the number
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE double fiftyoneDegreesUnalignedReadDouble(
	const void * const source) {
	double value;
	memcpy(&value, source, sizeof(value));
	return value;
}

/**
 * Writes an unsigned 32 bit integer to any byte.
 * @param destination address for the integer's first byte
 * @param value the integer
 */
FIFTYONE_DEGREES_UNALIGNED_INLINE void fiftyoneDegreesUnalignedWriteUint32(
	void * const destination,
	const uint32_t value) {
	memcpy(destination, &value, sizeof(value));
}

/**
 * @}
 */

#endif
