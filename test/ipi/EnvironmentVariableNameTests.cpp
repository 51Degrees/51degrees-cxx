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
 * @brief Tests that every environment variable, secret and CMake option the
 * files git tracks read is in the repository's list of names, docs/names.md,
 * and that every name there that 51Degrees defines starts _51DEGREES_. Also
 * tests that a workflow passes a shared workflow of common-ci its secrets with
 * secrets: inherit, and none by name.
 */

#include "pch.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/** The start of every name 51Degrees defines. */
static const std::string prefix = "_51DEGREES_";

/** The list of names, relative to the repository root. */
static const char namesFile[] = "docs/names.md";

/** The headings of the list, each over a table of names. */
static const std::string environmentHeading = "## Environment variables";
static const std::string secretsHeading = "## Secrets";
static const std::string optionsHeading = "## CMake options";
static const std::string otherHeading = "## Names other tools define";

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

static bool endsWith(const std::string &value, const std::string &end) {
	return value.size() >= end.size() &&
		value.compare(value.size() - end.size(), end.size(), end) == 0;
}

static bool startsWith(const std::string &value, const std::string &start) {
	return value.compare(0, start.size(), start) == 0;
}

/** The names in the first column of each table of the list, by the heading
 * the table is under. A name is written between backticks. */
static std::map<std::string, std::set<std::string>> listedNames(
	const fs::path &file) {
	std::map<std::string, std::set<std::string>> names;
	std::ifstream stream(file);
	std::string line, heading;
	while (std::getline(stream, line)) {
		if (line.empty() == false && line.back() == '\r') {
			line.pop_back();
		}
		if (startsWith(line, "## ")) {
			heading = line;
		}
		else if (startsWith(line, "| `")) {
			const size_t end = line.find('`', 3);
			if (end != std::string::npos) {
				names[heading].insert(line.substr(3, end - 3));
			}
		}
	}
	return names;
}

/** One name read in a file. */
struct Read {
	std::string where;
	std::string name;
};

/** Adds the first group of each match of the expression in the line. */
static void addMatches(
	const std::string &line,
	const std::regex &expression,
	const std::string &where,
	std::vector<Read> &found) {
	for (std::sregex_iterator it(line.begin(), line.end(), expression), end;
		it != end;
		++it) {
		found.push_back({ where, (*it)[1].str() });
	}
}

/** What the files read, each with the file and line it is read at. */
struct Reads {
	std::vector<Read> environment;
	std::vector<Read> secrets;
	std::vector<Read> options;
};

/** Adds what the file reads. C and C++ read the environment with getenv,
 * given the name or a macro of the file that holds it. CMake reads it as
 * $ENV{NAME} and PowerShell as $env:NAME. A workflow reads a secret as
 * secrets.NAME. CMake defines an option with option,
 * cmake_dependent_option or a cache variable. */
static void scan(
	const fs::path &file,
	const std::string &name,
	Reads &reads) {
	static const std::regex getenvName(
		"getenv\\s*\\(\\s*\"([A-Za-z0-9_]+)\"");
	static const std::regex getenvMacro(
		"getenv\\s*\\(\\s*([A-Z][A-Z0-9_]*)\\s*\\)");
	static const std::regex define(
		"#\\s*define\\s+([A-Z][A-Z0-9_]*)\\s+\"([A-Za-z0-9_]+)\"");
	static const std::regex cmakeEnv("\\$ENV\\{([A-Za-z0-9_]+)\\}");
	static const std::regex shellEnv("\\$[Ee]nv:([A-Za-z0-9_]+)");
	static const std::regex secret("secrets\\.([A-Za-z0-9_]+)");
	static const std::regex option(
		"^\\s*(?:cmake_dependent_)?option\\s*\\(\\s*([A-Za-z0-9_]+)");
	static const std::regex cache(
		"^\\s*set\\s*\\(\\s*([A-Za-z0-9_]+)\\s.*\\sCACHE\\s");
	const bool code = endsWith(name, ".c") || endsWith(name, ".h") ||
		endsWith(name, ".cpp") || endsWith(name, ".hpp");
	const bool cmake = endsWith(name, "CMakeLists.txt") ||
		endsWith(name, ".cmake");
	const bool script = endsWith(name, ".ps1") || endsWith(name, ".yml");
	const bool workflow = endsWith(name, ".yml");
	if ((code || cmake || script) == false) {
		return;
	}
	std::ifstream stream(file, std::ios::binary);
	std::string line;
	std::map<std::string, std::string> macros;
	std::vector<Read> macroReads;
	int number = 0;
	while (std::getline(stream, line)) {
		number++;
		const std::string where = name + ":" + std::to_string(number);
		if (code && line.find("define") != std::string::npos) {
			std::smatch match;
			if (std::regex_search(line, match, define)) {
				macros[match[1].str()] = match[2].str();
			}
		}
		if (code && line.find("getenv") != std::string::npos) {
			addMatches(line, getenvName, where, reads.environment);
			addMatches(line, getenvMacro, where, macroReads);
		}
		if (cmake && line.find("$ENV{") != std::string::npos) {
			addMatches(line, cmakeEnv, where, reads.environment);
		}
		if (script && line.find("nv:") != std::string::npos) {
			addMatches(line, shellEnv, where, reads.environment);
		}
		if (workflow && line.find("secrets.") != std::string::npos) {
			addMatches(line, secret, where, reads.secrets);
		}
		if (cmake && line.find("ption") != std::string::npos) {
			addMatches(line, option, where, reads.options);
		}
		if (cmake && line.find("CACHE") != std::string::npos) {
			addMatches(line, cache, where, reads.options);
		}
	}
	// A macro given to getenv is read as the name it holds. One the file
	// does not define is reported under the macro's own name.
	for (const Read &read : macroReads) {
		const auto macro = macros.find(read.name);
		reads.environment.push_back({
			read.where,
			macro == macros.end() ? read.name : macro->second });
	}
}

/** The start of the workflow every call of common-ci names. */
static const std::string commonCi = "51Degrees/common-ci/";

/** The job of a workflow that is being read. */
struct Job {
	/** The indent of the keys of the job, or -1 before the first. */
	int indent = -1;
	bool callsCommonCi = false;
	bool passesSecrets = false;
	/** Where the secrets key is, and what follows it on its line. */
	std::string where;
	std::string value;
};

/** Counts the job when it calls common-ci, and adds it to the named ones
 * when its secrets are anything but inherit. */
static void endJob(const Job &job, int &calls, std::vector<Read> &named) {
	if (job.callsCommonCi) {
		calls++;
		if (job.passesSecrets && job.value != "inherit") {
			named.push_back({ job.where, "secrets: " + job.value });
		}
	}
}

/** Reads the jobs of a workflow that call a workflow of common-ci. The uses
 * and secrets keys of a job have the same indent, and the job ends at the
 * first line with less. */
static void scanCalls(
	const fs::path &file,
	const std::string &name,
	int &calls,
	std::vector<Read> &named) {
	static const std::string usesKey = "uses:";
	static const std::string secretsKey = "secrets:";
	std::ifstream stream(file, std::ios::binary);
	std::string line;
	Job job;
	int number = 0;
	while (std::getline(stream, line)) {
		number++;
		const size_t start = line.find_first_not_of(" ");
		if (start == std::string::npos ||
			line[start] == '#' ||
			line[start] == '\r') {
			continue;
		}
		const int indent = static_cast<int>(start);
		if (job.indent >= 0 && indent < job.indent) {
			endJob(job, calls, named);
			job = Job();
		}
		const bool uses = line.compare(start, usesKey.size(), usesKey) == 0;
		const bool secrets =
			line.compare(start, secretsKey.size(), secretsKey) == 0;
		if ((uses || secrets) == false) {
			continue;
		}
		if (job.indent < 0) {
			job.indent = indent;
		}
		if (indent != job.indent) {
			continue;
		}
		if (uses) {
			job.callsCommonCi =
				line.find(commonCi, start) != std::string::npos;
		}
		else {
			// What follows the key, without a comment or the spaces
			// around it.
			std::string value = line.substr(start + secretsKey.size());
			value = value.substr(0, value.find('#'));
			const size_t first = value.find_first_not_of(" \t\r");
			const size_t last = value.find_last_not_of(" \t\r");
			job.passesSecrets = true;
			job.where = name + ":" + std::to_string(number);
			job.value = first == std::string::npos ?
				"" : value.substr(first, last - first + 1);
		}
	}
	endJob(job, calls, named);
}

/** Writes each read whose name is in neither set. */
static std::string notListed(
	const std::vector<Read> &reads,
	const std::set<std::string> &ours,
	const std::set<std::string> &other) {
	std::ostringstream message;
	for (const Read &read : reads) {
		if (ours.count(read.name) == 0 && other.count(read.name) == 0) {
			message << read.where << ": " << read.name << "\n";
		}
	}
	return message.str();
}

/**
 * Check that every name the tracked files read is in the list, and that
 * every name in the list that 51Degrees defines starts _51DEGREES_.
 */
TEST(EnvironmentVariableNames, EveryNameReadIsListed) {
	const fs::path root = findRoot();
	ASSERT_FALSE(root.empty()) <<
		"The repository was not found above " << fs::current_path();
	const std::vector<std::string> files = trackedFiles(root);
	ASSERT_FALSE(files.empty()) << "git listed no files in " << root;
	std::map<std::string, std::set<std::string>> listed =
		listedNames(root / namesFile);
	const std::set<std::string> &environment = listed[environmentHeading];
	const std::set<std::string> &secrets = listed[secretsHeading];
	const std::set<std::string> &options = listed[optionsHeading];
	const std::set<std::string> &other = listed[otherHeading];
	ASSERT_FALSE(environment.empty()) <<
		namesFile << " lists no environment variables";
	ASSERT_FALSE(secrets.empty()) << namesFile << " lists no secrets";
	ASSERT_FALSE(options.empty()) << namesFile << " lists no CMake options";

	// Every name 51Degrees defines starts with the prefix.
	for (const std::set<std::string> *names :
		{ &environment, &secrets, &options }) {
		for (const std::string &name : *names) {
			EXPECT_TRUE(startsWith(name, prefix)) <<
				name << " in " << namesFile << " does not start " << prefix;
		}
	}

	Reads reads;
	for (const std::string &name : files) {
		const fs::path path = root / fs::u8path(name);
		if (fs::is_regular_file(path)) {
			scan(path, name, reads);
		}
	}
	EXPECT_FALSE(reads.environment.empty()) <<
		"No read of an environment variable was found";
	EXPECT_FALSE(reads.options.empty()) << "No CMake option was found";
	const std::string unlistedEnvironment =
		notListed(reads.environment, environment, other);
	EXPECT_TRUE(unlistedEnvironment.empty()) <<
		"Environment variables read that " << namesFile <<
		" does not list:\n" << unlistedEnvironment;
	const std::string unlistedSecrets =
		notListed(reads.secrets, secrets, other);
	EXPECT_TRUE(unlistedSecrets.empty()) <<
		"Secrets read that " << namesFile << " does not list:\n" <<
		unlistedSecrets;
	const std::string unlistedOptions =
		notListed(reads.options, options, other);
	EXPECT_TRUE(unlistedOptions.empty()) <<
		"CMake options defined that " << namesFile <<
		" does not list:\n" << unlistedOptions;
}

/**
 * Check that a job that calls a workflow of common-ci passes its secrets
 * with secrets: inherit, and none by name.
 */
TEST(EnvironmentVariableNames, CommonCiCallsInheritSecrets) {
	const fs::path root = findRoot();
	ASSERT_FALSE(root.empty()) <<
		"The repository was not found above " << fs::current_path();
	const std::vector<std::string> files = trackedFiles(root);
	ASSERT_FALSE(files.empty()) << "git listed no files in " << root;
	int calls = 0;
	std::vector<Read> named;
	for (const std::string &name : files) {
		const fs::path path = root / fs::u8path(name);
		if (startsWith(name, ".github/workflows/") &&
			endsWith(name, ".yml") &&
			fs::is_regular_file(path)) {
			scanCalls(path, name, calls, named);
		}
	}
	EXPECT_GT(calls, 0) << "No call of " << commonCi << " was found";
	std::ostringstream message;
	for (const Read &read : named) {
		message << read.where << ": " << read.name << "\n";
	}
	EXPECT_TRUE(named.empty()) <<
		"Calls of " << commonCi << " that do not pass secrets: inherit:\n" <<
		message.str();
}
