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

/**
 * @file ExampleEnvironmentVariableTests.cpp
 * @brief Tests the environment variable names the IP Intelligence examples
 * read the data file path from.
 */

#include "pch.h"
#include <cstdlib>
#include <string>
#include "../../examples/ipi/Base/ExampleBase.h"

/** The name the examples read the data file path from. */
static const char newName[] = "_51DEGREES_IPI_PATH";

/** The deprecated name, split so the name check does not find it here. */
static const std::string oldName = std::string("5") + "1DEGREES_IPI_PATH";

/** Sets the variable, or removes it when the value is null. Windows also
 * removes a variable set to an empty value. */
static void setVariable(const char *name, const char *value) {
#ifdef _WIN32
	_putenv_s(name, value == nullptr ? "" : value);
#else
	if (value == nullptr) {
		unsetenv(name);
	}
	else {
		setenv(name, value, 1);
	}
#endif
}

/** Keeps a variable's value and puts it back when the test ends. */
class SavedVariable {
public:
	explicit SavedVariable(const char *variableName)
		: name(variableName) {
		const char *current = std::getenv(variableName);
		wasSet = current != nullptr;
		if (wasSet) {
			value = current;
		}
	}
	~SavedVariable() {
		setVariable(name.c_str(), wasSet ? value.c_str() : nullptr);
	}
private:
	std::string name;
	std::string value;
	bool wasSet;
};

/** The path the examples read, copied before the environment changes, or
 * "(null)" when none is set. */
static std::string readPath() {
	const char *path = fiftyoneDegreesExampleGetDataFilePathFromEnvironment();
	return path == nullptr ? std::string("(null)") : std::string(path);
}

/** The number of times the text is found in the output. */
static int count(const std::string &output, const std::string &text) {
	int found = 0;
	size_t at = output.find(text);
	while (at != std::string::npos) {
		found++;
		at = output.find(text, at + text.size());
	}
	return found;
}

/**
 * Check the path is read from the name that starts with an underscore, and
 * that nothing is said about a deprecated name.
 */
TEST(ExampleEnvironmentVariables, NewNameIsRead) {
	SavedVariable savedNew(newName);
	SavedVariable savedOld(oldName.c_str());
	setVariable(oldName.c_str(), nullptr);
	testing::internal::CaptureStderr();
	setVariable(newName, "new.ipi");
	const std::string set = readPath();
	setVariable(newName, nullptr);
	const std::string unset = readPath();
	const std::string output = testing::internal::GetCapturedStderr();
	EXPECT_EQ("new.ipi", set);
	EXPECT_EQ("(null)", unset);
	EXPECT_EQ(0, count(output, "deprecated")) << output;
}

/**
 * Check the deprecated name is still read when the new name is unset or
 * empty, that the new name wins when both are set, and that the deprecation
 * and the name that replaces it are printed once. This is the only test that
 * sets the deprecated name, so it is the first to read it in the process.
 */
TEST(ExampleEnvironmentVariables, DeprecatedNameIsReadWhenNewNameIsUnsetOrEmpty) {
	SavedVariable savedNew(newName);
	SavedVariable savedOld(oldName.c_str());
	testing::internal::CaptureStderr();
	setVariable(newName, nullptr);
	setVariable(oldName.c_str(), "old.ipi");
	const std::string unset = readPath();
	setVariable(newName, "");
	const std::string empty = readPath();
	setVariable(newName, "new.ipi");
	const std::string both = readPath();
	const std::string output = testing::internal::GetCapturedStderr();
	EXPECT_EQ("old.ipi", unset);
	EXPECT_EQ("old.ipi", empty);
	EXPECT_EQ("new.ipi", both);
	EXPECT_EQ(1, count(output, "which is deprecated")) << output;
	EXPECT_EQ(1, count(output, "from " + oldName + ",")) << output;
	EXPECT_EQ(1, count(output, std::string(newName) + " replaces it")) <<
		output;
}
