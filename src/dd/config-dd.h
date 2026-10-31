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


#ifndef FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_H_INCLUDED
#define FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_H_INCLUDED

/**
 * @ingroup FiftyOneDegreesDeviceDetection
 * @defgroup FiftyOneDegreesConfigDeviceDetection Device Detection Config
 *
 * Configuration for building device detection data sets.
 *
 * ## Introduction
 *
 * Configuration structure extending #fiftyoneDegreesConfigBase type with
 * options specific to device detection. This adds configuration options around
 * how a User-Agent is handled.
 *
 * For further info see @link FiftyOneDegreesConfig @endlink
 *
 * @{
 */

#include <stdint.h>
#include "../common/config.h"

/**
 * Device detection configuration structure containing device detection
 * specific configuration options, and options that apply to structures and
 * methods in device detection libraries.
 */
typedef struct fiftyone_degrees_config_device_detecton_t {
	fiftyoneDegreesConfigBase b; /**< Base structure members */
	bool updateMatchedUserAgent; /**< True if the detection should record the
									matched characters from the target
									User-Agent */
	/**
	 * Limit in bytes on the length of a pseudo header, including the
	 * separators between its values, and on the matched User-Agent. The
	 * working memory for a pseudo header is allocated from this value when
	 * a set of results is created, and detection does not grow it. A pseudo
	 * header longer than the limit is therefore ignored and detection moves
	 * on to the next header. The limit applies whether or not
	 * updateMatchedUserAgent is true. A single header such as User-Agent does
	 * not use this memory, so it is processed whatever its length. To raise
	 * the limit set a larger value before the data set is created, which also
	 * raises the memory each set of results holds.
	 */
	size_t maxMatchedUserAgentLength;
	bool allowUnmatched; /**< True if there should be at least one matched node
						 in order for the results to be considered valid. By
						 default, this is false */
	bool processSpecialEvidence; /**< Some evidence requires additional 
									processing that doesn't need to be checked
									for if being used in an environment that
									doesn't generate it. For example; GHEV and 
									SUA query evidence. By default, this is 
									true. */
} fiftyoneDegreesConfigDeviceDetection;

/** Default value for the #FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE macro. */
#define FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE_DEFAULT true

/** Default value for allow unmatched used in the default configuration. */
#ifndef FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_UNMATCHED
#define FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_UNMATCHED false
#endif

#ifndef FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_SPECIAL_EVIDENCE
#define FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_SPECIAL_EVIDENCE true
#endif

/**
 * Update matched User-Agent setting used in the default configuration macro
 * #FIFTYONE_DEGREES_DEVICE_DETECTION_CONFIG_DEFAULT.
 */
#define FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE \
FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE_DEFAULT

/**
 * Default value for the #fiftyoneDegreesConfigDeviceDetection structure with
 * index. maxMatchedUserAgentLength defaults to 500, so a pseudo header longer
 * than 500 bytes is ignored unless the limit is raised.
 */
#define FIFTYONE_DEGREES_DEVICE_DETECTION_CONFIG_DEFAULT_WITH_INDEX \
	FIFTYONE_DEGREES_CONFIG_DEFAULT_WITH_INDEX, \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE, \
	500, /* Default limit on a pseudo header and the matched User-Agent */ \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_UNMATCHED, \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_SPECIAL_EVIDENCE

 /**
  * Default value for the #fiftyoneDegreesConfigDeviceDetection structure
  * without index. maxMatchedUserAgentLength defaults to 500, so a pseudo
  * header longer than 500 bytes is ignored unless the limit is raised.
  */
#define FIFTYONE_DEGREES_DEVICE_DETECTION_CONFIG_DEFAULT_NO_INDEX \
	FIFTYONE_DEGREES_CONFIG_DEFAULT_NO_INDEX, \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_UPDATE, \
	500, /* Default limit on a pseudo header and the matched User-Agent */ \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_UNMATCHED, \
	FIFTYONE_DEGREES_CONFIG_DEVICE_DETECTION_DEFAULT_SPECIAL_EVIDENCE

/**
 * @}
 */

#endif