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
 * @brief Tests that every 51Degrees environment variable name in the
 * repository starts with an underscore, because a POSIX shell cannot set a
 * name that starts with a digit.
 */

#include "pch.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/** The start of a name a shell cannot set, split so this file does not match
 * itself. */
static const std::string digitInitial = std::string("5") + "1DEGREES_";

/** The folders that hold the sources, scripts and documents. The data
 * submodules and build output are left out. */
static const char *const scannedFolders[] = {
	".github", "ci", "docs", "examples", "src", "test", "tools" };

/** The extensions of the files at the root that are scanned. */
static const char *const rootExtensions[] = {
	".md", ".txt", ".json", ".yml", ".ps1", ".sh" };

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

/** Adds each name in the file that starts with the digit to found. */
static void scan(
	const fs::path &root,
	const fs::path &file,
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
					fs::relative(file, root).generic_string() + ":" +
					std::to_string(number) + ": " +
					line.substr(at, end - at));
			}
			at = line.find(digitInitial, end);
		}
	}
}

/**
 * Check that no file names an environment variable starting with the digit,
 * which a POSIX shell refuses with "not a valid identifier".
 */
TEST(EnvironmentVariableNames, NoNameStartsWithADigit) {
	const fs::path root = findRoot();
	ASSERT_FALSE(root.empty()) <<
		"The repository was not found above " << fs::current_path();
	std::vector<std::string> found;
	for (const fs::directory_entry &entry : fs::directory_iterator(root)) {
		const std::string extension = entry.path().extension().string();
		for (const char *rootExtension : rootExtensions) {
			if (entry.is_regular_file() && extension == rootExtension) {
				scan(root, entry.path(), found);
			}
		}
	}
	for (const char *folder : scannedFolders) {
		const fs::path dir = root / folder;
		if (fs::is_directory(dir) == false) {
			continue;
		}
		for (const fs::directory_entry &entry :
			fs::recursive_directory_iterator(dir)) {
			if (entry.is_regular_file()) {
				scan(root, entry.path(), found);
			}
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
