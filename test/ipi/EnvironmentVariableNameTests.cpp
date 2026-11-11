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
 * name that starts with a digit.
 */

#include "pch.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/** The start of a name a shell cannot set, split so this file does not match
 * itself. */
static const std::string digitInitial = std::string("5") + "1DEGREES_";

static bool isNameChar(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '_';
}

/** The repository root, searched for upward from the working directory, or
 * empty when it is not found. */
static fs::path findRoot() {
	fs::path dir = fs::current_path();
	while (dir != dir.parent_path()) {
		if (fs::is_directory(dir / "examples" / "ipi") &&
			fs::is_directory(dir / "src" / "ipi")) {
			return dir;
		}
		dir = dir.parent_path();
	}
	return fs::path();
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

/** Adds each name in the file that starts with the digit to found. */
static void scan(
	const fs::path &file,
	const std::string &name,
	std::vector<std::string> &found) {
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
				found.push_back(
					name + ":" + std::to_string(number) + ": " +
					line.substr(at, end - at));
			}
			at = line.find(digitInitial, end);
		}
	}
}

/**
 * Check that no file git tracks names an environment variable starting with
 * the digit, which a POSIX shell refuses with "not a valid identifier".
 */
TEST(EnvironmentVariableNames, NoNameStartsWithADigit) {
	const fs::path root = findRoot();
	ASSERT_FALSE(root.empty()) <<
		"The repository was not found above " << fs::current_path();
	const std::vector<std::string> files = trackedFiles(root);
	ASSERT_FALSE(files.empty()) << "git listed no files in " << root;
	std::vector<std::string> found;
	for (const std::string &name : files) {
		const fs::path path = root / fs::u8path(name);
		if (fs::is_regular_file(path)) {
			scan(path, name, found);
		}
	}
	std::ostringstream message;
	for (const std::string &item : found) {
		message << item << "\n";
	}
	EXPECT_TRUE(found.empty()) << found.size() <<
		" names start with a digit, which a POSIX shell cannot set, so " <<
		"each must start with an underscore:\n" << message.str();
}
