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
 * @file EnvironmentVariableNameTests.cpp
 * @brief Tests that every 51Degrees environment variable name in the files
 * git tracks starts with an underscore, because a POSIX shell cannot set a
 * name that starts with a digit. A deprecated name that is still read is
 * allowed only in the file that reads it.
 */

#include "pch.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/** The start of a name a shell cannot set, split so this file does not match
 * itself. */
static const std::string digitInitial = std::string("5") + "1DEGREES_";

/** A deprecated name that starts with the digit and is still read when the
 * name that replaces it is unset or empty, and the one file that reads it.
 */
struct DeprecatedName {
	const char *file;
	std::string name;
};

static const DeprecatedName deprecatedNames[] = {
	{ "examples/ipi/Base/ExampleBase.c", digitInitial + "IPI_PATH" }
};

/** True when the name is a deprecated name and the file is the one that
 * reads it. */
static bool isDeprecatedName(
	const std::string &file,
	const std::string &name) {
	for (const DeprecatedName &deprecated : deprecatedNames) {
		if (file == deprecated.file && name == deprecated.name) {
			return true;
		}
	}
	return false;
}

static bool isNameChar(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '_';
}

/** The repository root, searched for upward from the working directory, or
 * empty when it is not found. */
static fs::path findRoot() {
	fs::path dir = fs::current_path();
	while (true) {
		if (fs::is_directory(dir / "examples" / "ipi") &&
			fs::is_directory(dir / "src" / "ipi")) {
			return dir;
		}
		if (dir == dir.parent_path()) {
			return fs::path();
		}
		dir = dir.parent_path();
	}
}

/** The files git tracks under root, as paths relative to root. A submodule is
 * listed as one entry, which is a folder rather than a file. */
static std::vector<std::string> trackedFiles(const fs::path &root) {
	std::vector<std::string> files;
	const std::string command =
		"git -C \"" + root.string() + "\" ls-files -z";
#ifdef _WIN32
	// Binary mode, so the separators are not read as text.
	FILE *pipe = _popen(command.c_str(), "rb");
#else
	FILE *pipe = popen(command.c_str(), "r");
#endif
	if (pipe == nullptr) {
		return files;
	}
	std::string name;
	int c;
	while ((c = fgetc(pipe)) != EOF) {
		if (c == '\0') {
			if (name.empty() == false) {
				files.push_back(name);
			}
			name.clear();
		}
		else {
			name.push_back(static_cast<char>(c));
		}
	}
#ifdef _WIN32
	_pclose(pipe);
#else
	pclose(pipe);
#endif
	return files;
}

/** Adds each name in the file that starts with the digit to found, or to
 * deprecated when it is a deprecated name in the file that reads it. */
static void scan(
	const fs::path &file,
	const std::string &name,
	std::vector<std::string> &found,
	std::set<std::string> &deprecated) {
	std::ifstream stream(file, std::ios::binary);
	std::string line;
	int number = 0;
	while (std::getline(stream, line)) {
		number++;
		size_t at = line.find(digitInitial);
		while (at != std::string::npos) {
			size_t end = at;
			while (end < line.size() && isNameChar(line[end])) {
				end++;
			}
			// A name such as _51DEGREES_X contains the start of a name that
			// begins with the digit, so only a match at the start of a name
			// counts.
			if (at == 0 || isNameChar(line[at - 1]) == false) {
				const std::string match = line.substr(at, end - at);
				if (isDeprecatedName(name, match)) {
					deprecated.insert(name + ":" + match);
				}
				else {
					found.push_back(
						name + ":" + std::to_string(number) + ": " + match);
				}
			}
			at = line.find(digitInitial, end);
		}
	}
}

/**
 * Check that no file git tracks names an environment variable starting with
 * the digit, which a POSIX shell refuses with "not a valid identifier",
 * other than a deprecated name in the file that reads it, and that each
 * deprecated name is still found there.
 */
TEST(EnvironmentVariableNames, NoNameStartsWithADigit) {
	const fs::path root = findRoot();
	ASSERT_FALSE(root.empty()) <<
		"The repository was not found above " << fs::current_path();
	const std::vector<std::string> files = trackedFiles(root);
	ASSERT_FALSE(files.empty()) << "git listed no files in " << root;
	std::vector<std::string> found;
	std::set<std::string> deprecated;
	for (const std::string &name : files) {
		const fs::path path = root / fs::u8path(name);
		if (fs::is_regular_file(path)) {
			scan(path, name, found, deprecated);
		}
	}
	std::ostringstream message;
	for (const std::string &item : found) {
		message << item << "\n";
	}
	EXPECT_TRUE(found.empty()) << found.size() <<
		" names start with a digit, which a POSIX shell cannot set, so " <<
		"each must start with an underscore:\n" << message.str();
	// An entry left after its name stops being read would let the name come
	// back in that file unnoticed.
	for (const DeprecatedName &entry : deprecatedNames) {
		EXPECT_TRUE(deprecated.find(std::string(entry.file) + ":" +
			entry.name) != deprecated.end()) << entry.name <<
			" is no longer found in " << entry.file <<
			", so remove it from deprecatedNames.";
	}
}
