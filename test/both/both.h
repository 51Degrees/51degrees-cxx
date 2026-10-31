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

#ifndef BOTH_PRODUCTS_H_INCLUDED
#define BOTH_PRODUCTS_H_INCLUDED

/* The functions main.c calls, which need no product's headers to declare. */

typedef enum {
	BOTH_PASSED,
	BOTH_FAILED,
	BOTH_NO_DATA
} BothResult;

/** Returns the address of the product's copy of a common global. */
const void* bothDdCommonGlobal(void);
const void* bothIpiCommonGlobal(void);

/**
 * Calls a common function that returns a structure whose layout differs between
 * the two copies, so a call bound to the wrong copy reads the wrong fields.
 * Returns BOTH_PASSED if the fields are right, otherwise BOTH_FAILED.
 */
BothResult bothDdCommonCall(void);
BothResult bothIpiCommonCall(void);

/** Loads the product's data file and looks up one property. */
BothResult bothDdLookup(void);
BothResult bothIpiLookup(void);

#ifdef __cplusplus
/** Does the same lookup through the product's C++ engine. */
BothResult bothDdEngineLookup(void);
BothResult bothIpiEngineLookup(void);
#endif

#endif
