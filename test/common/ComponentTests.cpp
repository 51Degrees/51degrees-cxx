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
 * @file ComponentTests.cpp
 * @brief Tests the list of components a data set builds from its collection
 * when the list cannot be allocated, and when the collection holds fewer
 * components than the list is asked for and there is no exception.
 */

#include "pch.h"
#include <array>
#include <vector>
#include "Base.hpp"
#include "TestUtils_SizeManager.hpp"
extern "C" {
#include "../../src/common/fiftyone.h"
#include "../../src/common/collectionKeyTypes.h"
#include "../../src/common/component.h"
#include "../../src/common/list.h"
}
#include "VariableSizeCollection.hpp"

/** The bytes of a component with no key values, as a data file holds them. */
typedef std::array<unsigned char,
	sizeof(fiftyoneDegreesComponent) -
	sizeof(fiftyoneDegreesComponentKeyValuePair)> ComponentBytes;

/** The components in the collection each test builds. */
static const uint32_t componentCount = 2;

/** How long a call is given on the small collection the tests build. */
static const int ComponentLimitSeconds = 30;

/** The allocations made so far by countingMalloc. */
static int mallocCalls = 0;

/** The allocation that fails, counting from one, or zero for none. */
static int mallocFailsOn = 0;

static void* FIFTYONE_DEGREES_CALL_CONV countingMalloc(size_t size) {
	return ++mallocCalls == mallocFailsOn ?
		nullptr : fiftyoneDegreesMemoryTrackingMalloc(size);
}

class ComponentTests : public Base {
protected:
	/** Gets components numbered from zero, with no key values. */
	static std::vector<ComponentBytes> components() {
		std::vector<ComponentBytes> values(componentCount);
		for (uint32_t i = 0; i < componentCount; i++) {
			values[i].fill(0);
			values[i][0] = (unsigned char)i;
		}
		return values;
	}
};

/**
 * Test that a list that cannot be allocated is reported in the exception, as
 * the method returns nothing.
 */
TEST_F(ComponentTests, InitList_NoMemoryForList_SetsException) {
	fiftyoneDegreesSetUpMemoryTracking();
	{
		VariableSizeCollection<ComponentBytes> collection(components());
		fiftyoneDegreesList list;
		fiftyoneDegreesListReset(&list);
		FIFTYONE_DEGREES_EXCEPTION_CREATE;
		mallocCalls = 0;
		mallocFailsOn = 1;
		fiftyoneDegreesMalloc = countingMalloc;
		fiftyoneDegreesComponentInitList(
			collection.getState()->collection,
			&list,
			componentCount,
			exception);
		fiftyoneDegreesMalloc = fiftyoneDegreesMemoryTrackingMalloc;
		EXPECT_EQ(
			FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
			exception->status);
		EXPECT_EQ((uint32_t)0, list.count);
		fiftyoneDegreesListFree(&list);
	}
	EXPECT_EQ((size_t)0, fiftyoneDegreesUnsetMemoryTracking());
}

/**
 * Test that a collection read out of range with no exception returns null,
 * rather than reading through the null exception to record the failure.
 */
TEST_F(ComponentTests, Get_OutOfRange_NoException_ReturnsNull) {
	VariableSizeCollection<ComponentBytes> collection(components());
	fiftyoneDegreesCollection *components =
		collection.getState()->collection;
	fiftyoneDegreesCollectionItem item;
	fiftyoneDegreesDataReset(&item.data);
	const fiftyoneDegreesCollectionKey key = {
		{ components->size },
		CollectionKeyType_Component,
	};
	EXPECT_EQ(nullptr, components->get(components, &key, &item, nullptr));
}

/**
 * Test that a list asked for more components than the collection holds stops
 * at the end of the collection when there is no exception, and that the list
 * is then short, which is how the caller sees it. With no exception nothing
 * else ends the loop.
 */
TEST_F(ComponentTests, InitList_FewerComponents_NoException_Returns) {
	VariableSizeCollection<ComponentBytes> collection(components());
	fiftyoneDegreesList list;
	fiftyoneDegreesListReset(&list);
	{
		TimeLimit limit(ComponentLimitSeconds);
		fiftyoneDegreesComponentInitList(
			collection.getState()->collection,
			&list,
			componentCount + 1,
			nullptr);
	}
	EXPECT_EQ(componentCount, list.count);
	fiftyoneDegreesListFree(&list);
}
