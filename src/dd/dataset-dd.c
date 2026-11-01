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

#include "dataset-dd.h"

#include <string.h>
#include "fiftyone.h"

#ifndef MIN
#ifdef min
#define MIN(a,b) min(a,b)
#else
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#endif

/** Name which asks for the device id. It is not stored in the data file. */
#define DEVICE_ID_PROPERTY "DeviceId"

/**
 * Returns true if the required properties name the device id. The fields are
 * evaluated as #fiftyoneDegreesPropertiesCreate evaluates them, so existing
 * properties name nothing here, and case is ignored.
 */
static bool isDeviceIdNamed(PropertiesRequired *properties) {
	const char *name;
	size_t length;
	int i;
	if (properties == NULL || properties->existing != NULL) {
		return false;
	}
	if (properties->array != NULL && properties->count > 0) {
		for (i = 0; i < properties->count; i++) {
			if (_stricmp(properties->array[i], DEVICE_ID_PROPERTY) == 0) {
				return true;
			}
		}
		return false;
	}
	name = properties->string;
	while (name != NULL && *name != '\0') {
		length = strcspn(name, FIFTYONE_DEGREES_PROPERTIES_SEPARATORS);
		if (length == strlen(DEVICE_ID_PROPERTY) &&
			_strnicmp(name, DEVICE_ID_PROPERTY, length) == 0) {
			return true;
		}
		name += length;
		name += strspn(name, FIFTYONE_DEGREES_PROPERTIES_SEPARATORS);
	}
	return false;
}

/**
 * True if any available property can be overridden, so a null list of them
 * means the list could not be allocated.
 */
static bool hasOverridableProperty(
	PropertiesAvailable *available,
	void *state,
	fiftyoneDegreesOverridesFilterMethod filter) {
	for (uint32_t i = 0; i < available->count; i++) {
		if (filter(state, i) == true) {
			return true;
		}
	}
	return false;
}

fiftyoneDegreesStatusCode 
fiftyoneDegreesDataSetDeviceDetectionInitPropertiesAndHeaders(
	fiftyoneDegreesDataSetDeviceDetection *dataSet,
	fiftyoneDegreesPropertiesRequired *properties,
	void *state,
	fiftyoneDegreesPropertiesGetMethod getPropertyMethod,
	fiftyoneDegreesHeadersGetMethod getHeaderMethod,
	fiftyoneDegreesOverridesFilterMethod overridesFilter,
	fiftyoneDegreesEvidencePropertiesGetMethod getEvidencePropertiesMethod,
	fiftyoneDegreesException* exception) {
	StatusCode status = DataSetInitProperties(
		&dataSet->b,
		properties,
		state,
		getPropertyMethod,
		getEvidencePropertiesMethod);

	// The device id needs the profile of every component and no stored
	// property, so naming it marks the list rather than adding to it. A reload
	// keeps the mark because it creates the new list from this one. An empty
	// list is then valid, and this status with a list means the list is empty.
	if (dataSet->b.available != NULL) {
		if (isDeviceIdNamed(properties)) {
			dataSet->b.available->allComponentsRequired = true;
		}
		if (status == REQ_PROP_NOT_PRESENT &&
			dataSet->b.available->allComponentsRequired) {
			status = SUCCESS;
		}
	}
	if (status != SUCCESS) {
		return status;
	}

	status = DataSetInitHeaders(
		&dataSet->b,
		state,
		getHeaderMethod,
		exception);
	if (status != SUCCESS) {
		return status;
	}

	// Work out the unique HTTP header index of the User-Agent field.
	dataSet->uniqueUserAgentHeaderIndex = HeaderGetIndex(
		dataSet->b.uniqueHeaders,
		"User-Agent",
		sizeof("User-Agent") - 1);

	// Iterate the available properties and determine if any are available to
	// be overridden in the evidence.
	dataSet->b.overridable = OverridePropertiesCreate(
		dataSet->b.available,
		true,
		state,
		overridesFilter);

	// The ghevHeaders member is initialised in 
	// fiftyoneDegreesGhevDeviceDetectionInit if required.
	dataSet->ghevHeaders = NULL;

	if (dataSet->b.overridable == NULL &&
		hasOverridableProperty(dataSet->b.available, state, overridesFilter)) {
		EXCEPTION_SET(INSUFFICIENT_MEMORY);
		return INSUFFICIENT_MEMORY;
	}

	return status;
}

void fiftyoneDegreesDataSetDeviceDetectionRelease(
	fiftyoneDegreesDataSetDeviceDetection *dataSet) {
	DataSetRelease(&dataSet->b);
}

void fiftyoneDegreesDataSetDeviceDetectionFree(
	fiftyoneDegreesDataSetDeviceDetection *dataSet) {
	if (dataSet->ghevHeaders != NULL) {
		Free(dataSet->ghevHeaders);
		dataSet->ghevHeaders = NULL;
	}
	DataSetFree(&dataSet->b);
}

fiftyoneDegreesDataSetDeviceDetection* 
fiftyoneDegreesDataSetDeviceDetectionGet(
	fiftyoneDegreesResourceManager *manager) {
	return (DataSetDeviceDetection*)DataSetGet(manager);
}

void fiftyoneDegreesDataSetDeviceDetectionReset(
	fiftyoneDegreesDataSetDeviceDetection *dataSet) {
	DataSetReset(&dataSet->b);
	dataSet->uniqueUserAgentHeaderIndex = 0;
	dataSet->ghevHeaders = NULL;
}
