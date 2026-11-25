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

#pragma once

#include <cstdlib>
#include <string>

/**
 * Reads an environment variable. MSVC deprecates getenv, so _dupenv_s is
 * used there, and getenv everywhere else.
 * @param name of the variable to read
 * @param value set to the value of the variable when it is set, and left
 * unchanged when it is not
 * @return true if the variable is set, otherwise false
 */
inline bool readEnvironmentVariable(const char *name, std::string &value) {
#ifdef _MSC_VER
	char *buffer = nullptr;
	size_t length = 0;
	if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
		free(buffer);
		return false;
	}
	value = buffer;
	free(buffer);
	return true;
#else
	const char *current = std::getenv(name);
	if (current == nullptr) {
		return false;
	}
	value = current;
	return true;
#endif
}

/**
 * @return true if the CI environment variable is set to "true"
 */
inline bool isRunningOnCI() {
	std::string ci;
	return readEnvironmentVariable("CI", ci) && ci == "true";
}
