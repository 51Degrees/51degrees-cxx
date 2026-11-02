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
 * @file IpiFirstReadFileTests.cpp
 * @brief Tests that a graph which cannot be read from the data file fails the
 * first read of a property of its component, rather than the population of
 * the results, when the data set reads from file.
 *
 * The graphs are read from the file and the properties, values and strings
 * are held in memory. Once the manager is initialised, a copy of the data
 * file is cut short where its first graph begins, so every graph read fails
 * while everything else can still be read, wherever the file puts it.
 */

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"

/** The data file committed to the data submodule, so every build has it. */
static const char firstReadFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** An address the data file has an answer for. */
static const char firstReadFileAddress[] = "1.1.1.1";

class IpiFirstReadFileTests : public Base {
public:
	void SetUp() override {
		Base::SetUp();
		const std::string path =
			GetFilePath(_dataFolderName, firstReadFileName);
		if (path.empty()) {
			GTEST_SKIP() << "The file '" << firstReadFileName <<
				"' was not found. Run 'git submodule update --init' to "
				"fetch it.";
		}
		copyPath = std::string("ipi-first-read-") +
			::testing::UnitTest::GetInstance()->current_test_info()->name() +
			".ipi";
		std::filesystem::copy_file(
			path,
			copyPath,
			std::filesystem::copy_options::overwrite_existing);
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove(copyPath, error);
		// Released here as the memory check in the base class would otherwise
		// count it as a leak.
		std::string().swap(copyPath);
		Base::TearDown();
	}

protected:
	std::string copyPath;

	/** The first byte of the data file that any graph is read from. */
	static uint64_t firstGraphByte(const fiftyoneDegreesDataSetIpi* dataSet) {
		uint64_t first = UINT64_MAX;
		for (uint32_t i = 0; i < dataSet->graphsArray->count; i++) {
			const fiftyoneDegreesIpiCgInfo* info =
				&dataSet->graphsArray->items[i].info;
			const uint64_t starts[] = {
				(uint64_t)info->spanBytes.startPosition,
				(uint64_t)info->spans.startPosition,
				(uint64_t)info->clusters.startPosition,
				(uint64_t)info->nodes.collection.startPosition };
			for (const uint64_t start : starts) {
				if (start < first) {
					first = start;
				}
			}
		}
		return first;
	}

	/** True when the property is mandatory with a default value. */
	static bool mandatoryWithDefault(
		fiftyoneDegreesDataSetIpi* dataSet,
		int requiredPropertyIndex) {
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		const fiftyoneDegreesProperty* property = fiftyoneDegreesPropertyGet(
			dataSet->properties,
			dataSet->b.b.available->items[requiredPropertyIndex].propertyIndex,
			&item,
			exception);
		FIFTYONE_DEGREES_EXCEPTION_THROW;
		const bool result = property != nullptr &&
			property->isMandatory &&
			property->defaultValueIndex != UINT32_MAX;
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
		return result;
	}
};

/**
 * With the low memory profile every graph is read from the file, and here the
 * properties, values and strings are held in memory. Populating the results
 * reads no graph, the first read of a property reports the failed graph read,
 * and the component is not walked again for the results, so later reads find
 * no profile, giving no value or the default value of a mandatory property.
 */
TEST_F(IpiFirstReadFileTests, LowMemory_FailedGraphReadShowsAtFirstRead) {
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
	config.properties.loaded = true;
	config.values.loaded = true;
	config.strings.loaded = true;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	FIFTYONE_DEGREES_EXCEPTION_CREATE;
	ASSERT_EQ(
		FIFTYONE_DEGREES_STATUS_SUCCESS,
		fiftyoneDegreesIpiInitManagerFromFile(
			&manager,
			&config,
			&properties,
			copyPath.c_str(),
			exception));

	fiftyoneDegreesDataSetIpi* dataSet =
		fiftyoneDegreesDataSetIpiGet(&manager);
	const uint64_t cut = firstGraphByte(dataSet);
	const int property = 0;
	const bool hasDefault = mandatoryWithDefault(dataSet, property);
	fiftyoneDegreesDataSetIpiRelease(dataSet);
	if (cut == UINT64_MAX) {
		fiftyoneDegreesResourceManagerFree(&manager);
		FAIL() << "The data file has no graph.";
	}
	std::filesystem::resize_file(copyPath, cut);

	fiftyoneDegreesResultsIpi* results =
		fiftyoneDegreesResultsIpiCreate(&manager);
	fiftyoneDegreesResultsIpiFromIpAddressString(
		results,
		firstReadFileAddress,
		strlen(firstReadFileAddress),
		exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"Populating the results read a graph.";

	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	fiftyoneDegreesResultsIpiGetValues(results, property, exception);
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_FAILED) <<
		"The first read did not report the failed graph read.";

	FIFTYONE_DEGREES_EXCEPTION_CLEAR;
	EXPECT_EQ(
		hasDefault,
		fiftyoneDegreesResultsIpiGetHasValues(results, property, exception)) <<
		"A later read did not read as if the component had no profile.";
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY) <<
		"A later read walked the graph again.";
	EXPECT_EQ(
		FIFTYONE_DEGREES_RESULTS_NO_VALUE_REASON_NULL_PROFILE,
		fiftyoneDegreesResultsIpiGetNoValueReason(
			results,
			property,
			exception));
	EXPECT_TRUE(FIFTYONE_DEGREES_EXCEPTION_OKAY);

	fiftyoneDegreesResultsIpiFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}
