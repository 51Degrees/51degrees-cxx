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

#include "../../common/pch.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/fiftyone.h"

using namespace std;

/**
 * Checks that a detection ends when the graph nodes it reads form a cycle.
 *
 * A valid data file holds graphs with no cycles, but the nodes a detection
 * reads are not always the nodes in the file. A streaming profile reads them
 * on demand through file handles, and where those handles are shared, for
 * example by processes forked after the data set was created, one process
 * moves the read position of another and a node is read from the wrong place.
 * The offsets in such a node point anywhere, including back to a node already
 * visited, and the walk then never reaches a leaf. A detection must report
 * corrupt data in that case rather than run forever.
 */
class HashGraphCycleTests : public Base {
public:
	HashGraphCycleTests() {
		// The free Lite file is used so that the test runs the same way
		// wherever it runs, as the graphs differ between data files.
		dataFilePath = GetFilePath(
			_dataFolderName,
			"51Degrees-LiteV4.1.hash");
	}

protected:
	string dataFilePath;

	/**
	 * Points every route out of every node back at the node itself, which is
	 * the simplest cycle a corrupt read can produce. The slots of a hash table
	 * with a code of zero hold bucket positions rather than node offsets, so
	 * they are left alone.
	 */
	static void pointEveryNodeAtItself(
		fiftyoneDegreesDataSetHash *dataSet,
		fiftyoneDegreesException *exception) {
		uint32_t offset = 0;
		while (offset < dataSet->header.nodes.length) {
			fiftyoneDegreesCollectionItem item;
			fiftyoneDegreesDataReset(&item.data);
			fiftyoneDegreesGraphNode * const node =
				fiftyoneDegreesGraphGetNode(
					dataSet->nodes,
					offset,
					&item,
					exception);
			ASSERT_TRUE(node != NULL) << "No node at offset " << offset;
			fiftyoneDegreesGraphNodeHash * const hashes =
				(fiftyoneDegreesGraphNodeHash*)(node + 1);
			if (offset > 0) {
				node->unmatchedNodeOffset = (int32_t)offset;
				for (int32_t i = 0; i < node->hashesCount; i++) {
					if (hashes[i].hashCode != 0) {
						hashes[i].nodeOffset = (int32_t)offset;
					}
				}
			}
			const uint32_t nodeLength =
				(uint32_t)sizeof(fiftyoneDegreesGraphNode) +
				(uint32_t)(node->hashesCount *
					sizeof(fiftyoneDegreesGraphNodeHash));
			TEST_COLLECTION_RELEASE(dataSet->nodes, item);
			offset += nodeLength;
		}
	}
};

/*
 * Loads the data file in memory, where the nodes can be changed, makes every
 * node lead back to itself, and then detects a User-Agent. The detection runs
 * on another thread so that a walk which never ends is reported as a failure
 * after a time limit, rather than holding the test run. A thread that never
 * ends cannot be stopped, so the process exits with a failure in that case.
 */
TEST_F(HashGraphCycleTests, CycleEndsWithCorruptData)
{
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The Lite Hash data file was not found.";
	}
	EXCEPTION_CREATE;
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	fiftyoneDegreesPropertiesRequired properties =
		fiftyoneDegreesPropertiesDefault;
	properties.string = "IsMobile";
	fiftyoneDegreesHashInitManagerFromFile(
		&manager,
		&config,
		&properties,
		dataFilePath.c_str(),
		exception);
	EXCEPTION_THROW;

	fiftyoneDegreesDataSetHash * const dataSet =
		fiftyoneDegreesDataSetHashGet(&manager);
	pointEveryNodeAtItself(dataSet, exception);
	fiftyoneDegreesDataSetHashRelease(dataSet);
	ASSERT_FALSE(HasFatalFailure());

	fiftyoneDegreesResultsHash * const results =
		fiftyoneDegreesResultsHashCreate(&manager, 0, 0);
	const string userAgent =
		"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
		"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
		"Safari/9537.53";
	fiftyoneDegreesException detectionException;
	detectionException.file = NULL;
	detectionException.func = NULL;
	detectionException.line = 0;
	detectionException.status = FIFTYONE_DEGREES_STATUS_SUCCESS;
	// The graphs are walked when a result is first read rather than when the
	// evidence is added, so the device id is read to make the walk happen.
	char deviceId[256];
	auto detection = std::async(std::launch::async, [&]() {
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			userAgent.c_str(),
			userAgent.length(),
			&detectionException);
		if (detectionException.status == FIFTYONE_DEGREES_STATUS_SUCCESS) {
			fiftyoneDegreesHashGetDeviceIdFromResults(
				results,
				deviceId,
				sizeof(deviceId),
				&detectionException);
		}
	});
	if (detection.wait_for(std::chrono::seconds(60)) !=
		std::future_status::ready) {
		fprintf(stderr,
			"HashGraphCycleTests: the detection was still walking a graph "
			"with a cycle after 60 seconds.\n");
		fflush(stderr);
		std::_Exit(EXIT_FAILURE);
	}
	detection.get();

	EXPECT_EQ(
		FIFTYONE_DEGREES_STATUS_CORRUPT_DATA,
		detectionException.status) <<
		"A walk which visits more nodes than the graph holds must report "
		"corrupt data.";
	fiftyoneDegreesResultsHashFree(results);
	fiftyoneDegreesResourceManagerFree(&manager);
}
