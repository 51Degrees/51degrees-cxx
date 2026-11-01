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
 * @file HashShortFileTests.cpp
 * @brief Cuts the data file short when the tests run, just after its header,
 * inside its first collection, half way and one byte short of its end, and
 * loads each cut in every profile from a file and from memory, and as a
 * reload of a manager holding the whole file. Each must be refused with
 * CORRUPT_DATA, and a reload must keep the data set in use.
 */

#include "../../common/pch.h"
#include <cstdio>
#include <string>
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/hash.h"

class HashShortFileTests : public Base {
protected:
	std::vector<unsigned char> data;
	std::string wholePath;
	std::string cutPath;

	void SetUp() override {
		Base::SetUp();
		std::string found;
		for (int i = _HashFileNamesLength - 1; i >= 0 && found.empty(); i--) {
			found = GetFilePath(_dataFolderName, _HashFileNames[i]);
		}
		ASSERT_FALSE(found.empty()) << "No data file was found.";
		FILE *file = fopen(found.c_str(), "rb");
		ASSERT_NE(nullptr, file) << "Could not open '" << found << "'.";
		fseek(file, 0, SEEK_END);
		data.resize((size_t)ftell(file));
		rewind(file);
		const size_t bytes = fread(data.data(), 1, data.size(), file);
		fclose(file);
		ASSERT_EQ(data.size(), bytes) << "Could not read '" << found << "'.";
		const std::string name =
			::testing::UnitTest::GetInstance()->current_test_info()->name();
		wholePath = "hash-short-whole-" + name + ".hash";
		cutPath = "hash-short-cut-" + name + ".hash";
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesFileWrite(
				wholePath.c_str(),
				data.data(),
				data.size()));
	}

	void TearDown() override {
		remove(wholePath.c_str());
		remove(cutPath.c_str());
		// Released here as the memory check in the base class would otherwise
		// count them as leaks.
		std::vector<unsigned char>().swap(data);
		std::string().swap(wholePath);
		std::string().swap(cutPath);
		Base::TearDown();
	}

	/**
	 * Gets the lengths to cut the data file to.
	 */
	std::vector<size_t> cuts() const {
		const fiftyoneDegreesDataSetHashHeader *header =
			(const fiftyoneDegreesDataSetHashHeader*)data.data();
		return {
			sizeof(fiftyoneDegreesDataSetHashHeader),
			(size_t)header->strings.startPosition +
				header->strings.length / 2,
			data.size() / 2,
			data.size() - 1,
		};
	}

	static fiftyoneDegreesStatusCode initFromFile(
		fiftyoneDegreesConfigHash config,
		const std::string &path) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				path.c_str(),
				exception);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		return status;
	}

	static fiftyoneDegreesStatusCode initFromMemory(
		fiftyoneDegreesConfigHash config,
		std::vector<unsigned char> &memory) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		const fiftyoneDegreesStatusCode status =
			fiftyoneDegreesHashInitManagerFromMemory(
				&manager,
				&config,
				&properties,
				memory.data(),
				(long)memory.size(),
				exception);
		if (status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		return status;
	}

	/**
	 * Expects a reload of the cut, from its file or from memory, to be
	 * refused by a manager holding the whole file, which keeps its data set.
	 */
	void expectReloadRefused(
		fiftyoneDegreesConfigHash config,
		std::vector<unsigned char> &cut,
		const bool fromMemory,
		const std::string &where) {
		fiftyoneDegreesResourceManager manager;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesHashInitManagerFromFile(
				&manager,
				&config,
				&properties,
				wholePath.c_str(),
				exception));
		const void *active = (const void*)manager.active;
		const fiftyoneDegreesStatusCode status = fromMemory ?
			fiftyoneDegreesHashReloadManagerFromMemory(
				&manager,
				cut.data(),
				(long)cut.size(),
				exception) :
			fiftyoneDegreesHashReloadManagerFromFile(
				&manager,
				cutPath.c_str(),
				exception);
		EXPECT_EQ(FIFTYONE_DEGREES_STATUS_CORRUPT_DATA, status) << where <<
			(fromMemory ? "reload from memory" : "reload from the file");
		EXPECT_EQ(active, (const void*)manager.active) << where <<
			(fromMemory ? "reload from memory" : "reload from the file");
		fiftyoneDegreesResourceManagerFree(&manager);
	}

	/**
	 * Expects each cut to be refused in the profile, in each way it can be
	 * loaded.
	 */
	void expectEachCutRefused(const fiftyoneDegreesConfigHash &config) {
		for (const size_t length : cuts()) {
			// The cut is named in each message rather than traced, as a trace
			// keeps memory the check in a Windows debug build counts.
			const std::string where = "Cut to " + std::to_string(length) +
				" of " + std::to_string(data.size()) + " bytes, ";
			std::vector<unsigned char> cut(data.begin(), data.begin() + length);
			ASSERT_EQ(
				FIFTYONE_DEGREES_STATUS_SUCCESS,
				fiftyoneDegreesFileWrite(
					cutPath.c_str(),
					cut.data(),
					cut.size())) << where;
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
				initFromFile(config, cutPath)) << where << "load from the file";
			EXPECT_EQ(
				FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
				initFromMemory(config, cut)) << where << "load from memory";
			expectReloadRefused(config, cut, false, where);
			expectReloadRefused(config, cut, true, where);
		}
	}
};

TEST_F(HashShortFileTests, MaxPerformance_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashInMemoryConfig);
}

TEST_F(HashShortFileTests, HighPerformance_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashHighPerformanceConfig);
}

TEST_F(HashShortFileTests, LowMemory_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashLowMemoryConfig);
}

TEST_F(HashShortFileTests, Balanced_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashBalancedConfig);
}

TEST_F(HashShortFileTests, BalancedTemp_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashBalancedTempConfig);
}

TEST_F(HashShortFileTests, Default_EachCut_IsRefused) {
	expectEachCutRefused(fiftyoneDegreesHashDefaultConfig);
}
