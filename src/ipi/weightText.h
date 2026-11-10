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

#ifndef FIFTYONE_DEGREES_IPI_WEIGHT_TEXT_H_INCLUDED
#define FIFTYONE_DEGREES_IPI_WEIGHT_TEXT_H_INCLUDED

/**
 * @file weightText.h
 * Writes the weight that fiftyoneDegreesResultsIpiGetValueTexts puts after a
 * value's colon. It is used inside the IP Intelligence library and by its
 * tests, and is not part of the API that ipi.h gives.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The most characters fiftyoneDegreesIpiGetWeightText writes, as in
 * `2.32838e-10` for a raw weighting of 1.
 */
#define FIFTYONE_DEGREES_IPI_WEIGHT_TEXT_LENGTH 11

/**
 * Writes a weight as fiftyoneDegreesResultsIpiGetValueTexts writes it after a
 * value's colon. The weight is the raw weighting over
 * FIFTYONE_DEGREES_WEIGHTED_ITEM_MAX_WEIGHT, written to six significant
 * figures with no trailing zeros, and with an exponent when it rounds to less
 * than 0.0001, as printf's %g and a C++ stream write a double. No null is
 * written after it.
 * @param text where the weight is written, with room for
 * FIFTYONE_DEGREES_IPI_WEIGHT_TEXT_LENGTH characters
 * @param rawWeighting the weight out of
 * FIFTYONE_DEGREES_WEIGHTED_ITEM_MAX_WEIGHT
 * @return the number of characters written
 */
size_t fiftyoneDegreesIpiGetWeightText(
	char* text,
	uint32_t rawWeighting);

#ifdef __cplusplus
}
#endif

#endif
