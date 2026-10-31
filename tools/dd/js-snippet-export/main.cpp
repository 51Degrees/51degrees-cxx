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
 * @file main.cpp
 * @brief Writes the JavaScript snippets in a Hash data file to files, with a
 * manifest.json that lists them.
 */

#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../../../src/dd/hash/EngineHash.hpp"

using namespace FiftyoneDegrees::Common;
using namespace FiftyoneDegrees::DeviceDetection::Hash;
namespace fs = std::filesystem;

static int usage(int exitCode) {
	std::cerr << "Usage: js-snippet-export -d <data_file> [-o <output_dir>]\n"
		"  -d  Path to the Hash data file\n"
		"  -o  Output directory (default: js-snippets)\n";
	return exitCode;
}

static std::string toFileName(const std::string& propertyName) {
	std::string result;
	for (const char c : propertyName) {
		const unsigned char u = static_cast<unsigned char>(c);
		if (std::isalnum(u)) {
			result += static_cast<char>(std::tolower(u));
		}
		else if (c == ' ' || c == '_' || c == '-') {
			result += '_';
		}
	}
	return result;
}

static std::string toJsonString(const std::string& text) {
	std::ostringstream json;
	json << '"';
	for (const char c : text) {
		if (c == '"' || c == '\\') {
			json << '\\' << c;
		}
		else if (static_cast<unsigned char>(c) < 0x20) {
			json << "\\u" << std::hex << std::setw(4) << std::setfill('0')
				<< static_cast<int>(c);
		}
		else {
			json << c;
		}
	}
	json << '"';
	return json.str();
}

// Binary, so the file holds the bytes from the data file on every platform.
// The close is checked because a full disk may only show when the file flushes.
static void writeFile(const fs::path& path, const std::string& content) {
	std::ofstream file(path, std::ios::binary);
	file << content;
	file.close();
	if (file.fail()) {
		throw std::runtime_error("Failed to write " + path.u8string());
	}
}

int main(int argc, char* argv[]) {
	fs::path dataFile;
	fs::path outputDir = "js-snippets";
	for (int i = 1; i < argc; i++) {
		const std::string arg = argv[i];
		if (arg == "-d" && i + 1 < argc) {
			dataFile = argv[++i];
		}
		else if (arg == "-o" && i + 1 < argc) {
			outputDir = argv[++i];
		}
		else {
			return usage(arg == "-h" || arg == "--help" ? 0 : 1);
		}
	}
	if (dataFile.empty()) {
		return usage(1);
	}

	try {
		ConfigHash config;
		RequiredPropertiesConfig properties;
		EngineHash engine(dataFile.string(), &config, &properties);
		// Can return false for a path ending in a separator even when the
		// folder was created, so only a thrown error counts as a failure.
		fs::create_directories(outputDir);

		std::unique_ptr<Collection<std::string, PropertyMetaData>> all(
			engine.getMetaData()->getProperties());
		std::ostringstream snippets;
		uint32_t propertyCount = 0;
		uint32_t snippetCount = 0;
		for (uint32_t i = 0; i < all->getSize(); i++) {
			std::unique_ptr<PropertyMetaData> property(all->getByIndex(i));
			if (property->getType() != "javascript") {
				continue;
			}
			propertyCount++;
			std::unique_ptr<Collection<ValueMetaDataKey, ValueMetaData>> values(
				engine.getMetaData()->getValuesForProperty(property.get()));
			for (uint32_t j = 0; j < values->getSize(); j++) {
				std::unique_ptr<ValueMetaData> value(values->getByIndex(j));
				const std::string snippet = value->getName();
				if (snippet.empty()) {
					continue;
				}
				const std::string file = toFileName(property->getName()) +
					"_" + std::to_string(j) + ".js";
				writeFile(outputDir / file, snippet);
				snippets << (snippetCount++ > 0 ? ",\n" : "")
					<< "    { \"file\": " << toJsonString(file)
					<< ", \"property\": " << toJsonString(property->getName())
					<< ", \"index\": " << j << " }";
			}
		}

		const std::time_t now = std::time(nullptr);
		std::ostringstream manifest;
		manifest << "{\n  \"generated\": \""
			<< std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ") << "\",\n"
			<< "  \"dataFile\": " << toJsonString(dataFile.u8string()) << ",\n"
			<< "  \"jsPropertyCount\": " << propertyCount << ",\n"
			<< "  \"snippetCount\": " << snippetCount << ",\n"
			<< "  \"snippets\": [\n" << snippets.str() << "\n  ]\n}\n";
		writeFile(outputDir / "manifest.json", manifest.str());

		std::cout << "Export complete! " << snippetCount << " snippets from "
			<< propertyCount << " JavaScript properties." << std::endl;
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
