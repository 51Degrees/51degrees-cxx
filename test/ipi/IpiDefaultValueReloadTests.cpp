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
 * @file IpiDefaultValueReloadTests.cpp
 * @brief Checks that the default value the metadata gives a property is the
 * default in the data set in use, when the property's metadata was read
 * before a reload that changed that default.
 *
 * The data set is reloaded from a copy of the free ASN data file in which
 * one property's default value index is moved to another of its values.
 */

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using std::string;
using std::unique_ptr;
using std::vector;
namespace Ipi = FiftyoneDegrees::IpIntelligence;
namespace Common = FiftyoneDegrees::Common;

using Properties = Common::Collection<string, Common::PropertyMetaData>;

/**
 * The free ASN data file, which the data repository's script downloads, and
 * which is small enough to hold two copies of in memory.
 */
static const char reloadFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";

/** The property whose default the reloaded copy moves. */
static const char reloadPropertyName[] = "Asn";

/** The bytes of the file. */
static vector<unsigned char> readReloadFile(const string &path) {
	std::ifstream file(path, std::ios::binary);
	return vector<unsigned char>(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
}

/**
 * A manager over every property of the data file in `data`, which the data
 * set reads in place, so `data` must outlive the manager.
 */
static std::shared_ptr<fiftyoneDegreesResourceManager> openFromMemory(
	vector<unsigned char> &data) {
	fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiInMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	fiftyoneDegreesResourceManager *manager =
		new fiftyoneDegreesResourceManager();
	EXCEPTION_CREATE;
	const fiftyoneDegreesStatusCode status =
		fiftyoneDegreesIpiInitManagerFromMemory(
			manager,
			&config,
			&properties,
			data.data(),
			(fiftyoneDegreesFileOffset)data.size(),
			exception);
	if (status != FIFTYONE_DEGREES_STATUS_SUCCESS || EXCEPTION_FAILED) {
		delete manager;
		throw Common::StatusCodeException(status);
	}
	return std::shared_ptr<fiftyoneDegreesResourceManager>(
		manager,
		[](fiftyoneDegreesResourceManager *m) {
			fiftyoneDegreesResourceManagerFree(m);
			delete m;
		});
}

/** The metadata of the property, read from the data set in use. */
static Common::PropertyMetaData* readProperty(
	const Ipi::MetaDataIpi &metaData) {
	unique_ptr<Properties> properties(metaData.getProperties());
	return properties->getByKey(reloadPropertyName);
}

/**
 * Where the property's default value index is in `data`, which the data set
 * of the manager reads in place, and another of the property's values to
 * move it to. Returns false when the property is not found in `data`.
 */
static bool findDefaultValueIndex(
	fiftyoneDegreesResourceManager *manager,
	const vector<unsigned char> &data,
	size_t *position,
	uint32_t *other) {
	bool found = false;
	fiftyoneDegreesDataSetIpi *dataSet = fiftyoneDegreesDataSetIpiGet(manager);
	EXCEPTION_CREATE;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesProperty *property =
		fiftyoneDegreesPropertyGetByName(
			dataSet->properties,
			dataSet->strings,
			reloadPropertyName,
			&item,
			exception);
	if (property != nullptr && EXCEPTION_OKAY) {
		const unsigned char *bytes = (const unsigned char*)property;
		if (bytes >= data.data() &&
			bytes + sizeof(fiftyoneDegreesProperty) <=
				data.data() + data.size()) {
			*position = (size_t)(bytes - data.data()) +
				offsetof(fiftyoneDegreesProperty, defaultValueIndex);
			*other = property->defaultValueIndex == property->firstValueIndex
				? property->lastValueIndex
				: property->firstValueIndex;
			found = *other != property->defaultValueIndex;
		}
		FIFTYONE_DEGREES_COLLECTION_RELEASE(dataSet->properties, &item);
	}
	fiftyoneDegreesDataSetIpiRelease(dataSet);
	return found;
}

class IpiDefaultValueReloadTests : public Base {};

TEST_F(IpiDefaultValueReloadTests, DefaultFromTheDataSetInUse) {
	const string path = GetFilePath(_dataFolderName, reloadFileName);
	if (path.empty()) {
		GTEST_SKIP() << "The ASN data file was not found.";
	}
	// The file is printed rather than added with SCOPED_TRACE, whose memory
	// the base class's memory check counts.
	printf("Checking %s\n", path.c_str());
	vector<unsigned char> first = readReloadFile(path);
	ASSERT_FALSE(first.empty());
	// Copied before the data set reads the first copy.
	vector<unsigned char> second = first;

	const auto manager = openFromMemory(first);
	Ipi::MetaDataIpi metaData(manager);
	unique_ptr<Common::PropertyMetaData> kept(readProperty(metaData));
	ASSERT_TRUE(kept != nullptr);

	size_t position = 0;
	uint32_t other = 0;
	ASSERT_TRUE(findDefaultValueIndex(
		manager.get(),
		first,
		&position,
		&other)) << "The property is read in place from the data file.";
	memcpy(second.data() + position, &other, sizeof(other));
	{
		EXCEPTION_CREATE;
		ASSERT_EQ(
			FIFTYONE_DEGREES_STATUS_SUCCESS,
			fiftyoneDegreesIpiReloadManagerFromMemory(
				manager.get(),
				second.data(),
				(fiftyoneDegreesFileOffset)second.size(),
				exception));
		ASSERT_TRUE(EXCEPTION_OKAY);
	}

	unique_ptr<Common::PropertyMetaData> reloaded(readProperty(metaData));
	ASSERT_TRUE(reloaded != nullptr);
	ASSERT_NE(kept->getDefaultValue(), reloaded->getDefaultValue()) <<
		"The reload moves the default.";

	// The metadata kept from before the reload gets the default of the data
	// set in use, as the metadata read after it does.
	unique_ptr<Common::ValueMetaData> fromKept(
		metaData.getDefaultValueForProperty(kept.get()));
	unique_ptr<Common::ValueMetaData> fromReloaded(
		metaData.getDefaultValueForProperty(reloaded.get()));
	ASSERT_TRUE(fromKept != nullptr);
	ASSERT_TRUE(fromReloaded != nullptr);
	EXPECT_EQ(reloaded->getDefaultValue(), fromKept->getName());
	EXPECT_EQ(reloaded->getDefaultValue(), fromReloaded->getName());
}
