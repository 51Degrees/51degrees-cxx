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

#include "RequiredPropertiesConfig.hpp"

#include "fiftyone.h"

using namespace FiftyoneDegrees::Common;

#ifdef _MSC_FULL_VER
#include <string.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif

/**
 * Finds the next property in a comma separated list without changing the
 * list. Empty items are skipped.
 * @param current position in the list to start from
 * @param length set to the number of characters in the property found
 * @return the start of the property, or nullptr if there are no more
 */
static const char* nextProperty(const char *current, size_t *length) {
	while (*current == ',') {
		current++;
	}
	if (*current == '\0') {
		return nullptr;
	}
	const char *end = strchr(current, ',');
	*length = end == nullptr ? strlen(current) : (size_t)(end - current);
	return current;
}

RequiredPropertiesConfig::RequiredPropertiesConfig() {
	conf.count = 0;
	conf.array = nullptr;
	conf.existing = nullptr;
	conf.string = nullptr;
}

RequiredPropertiesConfig::RequiredPropertiesConfig(
	vector<string> *properties) : RequiredPropertiesConfig() {
	if (properties != nullptr) {
		conf.count = (int)properties->size();
		conf.array = (const char**)Malloc(
			conf.count * sizeof(const char*));
		for (int i = 0; i < conf.count; i++) {
			size_t size = (*properties)[i].size() + 1;
			conf.array[i] = (char*)Malloc(size);
			memcpy((char*)conf.array[i], (*properties)[i].c_str(), size);
		}
	}
}

RequiredPropertiesConfig::RequiredPropertiesConfig(
	const vector<string> *properties)
	: RequiredPropertiesConfig((vector<string>*)properties) {
}

RequiredPropertiesConfig::RequiredPropertiesConfig(
	const char *properties) : RequiredPropertiesConfig() {
	if (properties != nullptr) {
		size_t size = strlen(properties) + 1;
		conf.string = (const char*)Malloc(size);
		memcpy((char*)conf.string, properties, size);
	}
}

RequiredPropertiesConfig::RequiredPropertiesConfig(
	const string &properties)
	: RequiredPropertiesConfig(properties.c_str()) {}

RequiredPropertiesConfig::RequiredPropertiesConfig(
	const string *properties) : RequiredPropertiesConfig(
	properties == nullptr ? nullptr : properties->c_str()) {}

RequiredPropertiesConfig::~RequiredPropertiesConfig() {
	int i;
	if (conf.array != nullptr) {
		for (i = 0; i < conf.count; i++) {
			Free((void*)conf.array[i]);
		}
		Free(conf.array);
	}
	if (conf.string != nullptr) {
		Free((void*)conf.string);
	}
}

vector<string> RequiredPropertiesConfig::getProperties() const {
	int i;
	const char *current;
	size_t length = 0;
	vector<string> result;
	if (conf.array != nullptr) {
		for (i = 0; i < conf.count; i++) {
			result.insert(result.end(), conf.array[i]);
		}
	}
	else if (conf.string != nullptr) {
		current = nextProperty(conf.string, &length);
		while (current != nullptr) {
			result.insert(result.end(), string(current, length));
			current = nextProperty(current + length, &length);
		}
	}
	return result;
}

bool RequiredPropertiesConfig::containsProperty(
	const char *property) const {
	int i;
	const char *current;
	size_t length = 0;
	if (conf.array != nullptr) {
		for (i = 0; i < conf.count; i++) {
			if (_stricmp(property, conf.array[i]) == 0) {
				return true;
			}
		}
	}
	else if (conf.string != nullptr) {
		current = nextProperty(conf.string, &length);
		while (current != nullptr) {
			// Compare the whole property, ignoring case.
			if (_strnicmp(property, current, length) == 0 &&
				property[length] == '\0') {
				return true;
			}
			current = nextProperty(current + length, &length);
		}
	}
	return false;
}

bool RequiredPropertiesConfig::containsProperty(
	const string property) const {
	return containsProperty(property.c_str());
}

int RequiredPropertiesConfig::getCount() const {
	int count;
	char *current;
	if (conf.array != nullptr) {
		return conf.count;
	}
	else if (conf.string != nullptr) {
		count = 0;
		current = (char*)conf.string;
		while (*current != '\0') {
			if (*current == ',') {
				count++;
			}
			current++;
		}
		return count;
	}
	return 0;
}

PropertiesRequired* RequiredPropertiesConfig::getConfig() {
	return &conf;
}

