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

#ifndef FIFTYONE_DEGREES_COMMON_H_INCLUDED
#define FIFTYONE_DEGREES_COMMON_H_INCLUDED

#ifdef FIFTYONE_DEGREES_IPI_SYMBOLS
#include "ipiSymbols.h"
#endif

#ifdef _MSC_FULL_VER
#define DEPRECATED __declspec(deprecated)
#else
#define DEPRECATED __attribute__((deprecated))
#endif

#ifdef __cplusplus
#define EXTERNAL extern "C"
#else
#define EXTERNAL
#endif

#ifdef __cplusplus
#define EXTERNAL_VAR extern "C"
#else
#define EXTERNAL_VAR extern
#endif

// The characters at the start of a cookie or other storage key that indicate
// the item is related to 51Degrees.
#ifndef FIFTYONE_DEGREES_COMMON_COOKIE_PREFIX
#define FIFTYONE_DEGREES_COMMON_COOKIE_PREFIX "51D_"
#endif

// The common C++ headers open this inside their own namespace. It expands to
// nothing unless FIFTYONE_DEGREES_IPI_CXX_SYMBOLS is defined. It then changes
// the linked names of the classes in the IP Intelligence copy and leaves source
// names alone, because members of an inline namespace are found through the
// namespace around it.
#ifdef FIFTYONE_DEGREES_IPI_CXX_SYMBOLS
#define FIFTYONE_DEGREES_COMMON_CXX_BEGIN inline namespace IpiCopy {
#define FIFTYONE_DEGREES_COMMON_CXX_END }
#else
#define FIFTYONE_DEGREES_COMMON_CXX_BEGIN
#define FIFTYONE_DEGREES_COMMON_CXX_END
#endif

#endif
