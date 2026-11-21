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
#include <vector>
#include "../Constants.hpp"
#include "../../common/Base.hpp"
#include "../../../src/dd/hash/fiftyone.h"

using namespace std;

/**
 * Finds a set of root nodes by its index. The sets have a fixed size, so no
 * method is needed to work out the size.
 */
static const fiftyoneDegreesCollectionKeyType rootNodesKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(fiftyoneDegreesHashRootNodes),
	NULL,
};

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
	 * Adds the offsets of the root nodes that a detection from a User-Agent
	 * starts its walks at. Each component holds the index of a set of root
	 * nodes for each header it uses, and each set holds the root of the
	 * performance graph and the root of the predictive graph.
	 */
	static void addUserAgentRoots(
		fiftyoneDegreesDataSetHash *dataSet,
		vector<uint32_t> &offsets,
		fiftyoneDegreesException *exception) {
		const fiftyoneDegreesHeaderID headerId =
			dataSet->b.b.uniqueHeaders->items[
				dataSet->b.uniqueUserAgentHeaderIndex].headerId;
		for (uint32_t c = 0; c < dataSet->componentsList.count; c++) {
			fiftyoneDegreesComponent * const component =
				(fiftyoneDegreesComponent*)
				dataSet->componentsList.items[c].data.ptr;
			ASSERT_TRUE(component != NULL) << "No component at index " << c;
			for (uint16_t k = 0; k < component->keyValuesCount; k++) {
				if ((&component->firstKeyValuePair)[k].key != headerId) {
					continue;
				}
				const uint32_t index =
					(&component->firstKeyValuePair)[k].value;
				fiftyoneDegreesCollectionItem item;
				fiftyoneDegreesDataReset(&item.data);
				const fiftyoneDegreesCollectionKey key = {
					{ index },
					&rootNodesKeyType,
				};
				const fiftyoneDegreesHashRootNodes * const roots =
					(const fiftyoneDegreesHashRootNodes*)
					dataSet->rootNodes->get(
						dataSet->rootNodes,
						&key,
						&item,
						exception);
				ASSERT_TRUE(roots != NULL) <<
					"No root nodes at index " << index;
				offsets.push_back(roots->performanceNodeOffset);
				offsets.push_back(roots->predictiveNodeOffset);
				TEST_COLLECTION_RELEASE(dataSet->rootNodes, item);
			}
		}
	}

	/**
	 * Returns true if the hash record at the index is a route to another
	 * node. In a hash table a slot with a code of zero holds the position of
	 * a bucket rather than a node offset, and no search returns it.
	 */
	static bool isRoute(fiftyoneDegreesGraphNode *node, int32_t index) {
		const fiftyoneDegreesGraphNodeHash * const hashes =
			(const fiftyoneDegreesGraphNodeHash*)(node + 1);
		return node->modulo == 0 || hashes[index].hashCode != 0;
	}

	/**
	 * Points every route out of the node back at the node, which is the
	 * simplest cycle a corrupt read can make. A walk that reaches the node
	 * then reaches it again at every step, whatever the evidence matches.
	 */
	static void pointAtItself(
		fiftyoneDegreesDataSetHash *dataSet,
		uint32_t offset,
		fiftyoneDegreesException *exception) {
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		fiftyoneDegreesGraphNode * const node = fiftyoneDegreesGraphGetNode(
			dataSet->nodes,
			offset,
			&item,
			exception);
		ASSERT_TRUE(node != NULL) << "No node at offset " << offset;
		fiftyoneDegreesGraphNodeHash * const hashes =
			(fiftyoneDegreesGraphNodeHash*)(node + 1);
		node->unmatchedNodeOffset = (int32_t)offset;
		for (int32_t i = 0; i < node->hashesCount; i++) {
			if (isRoute(node, i)) {
				hashes[i].nodeOffset = (int32_t)offset;
			}
		}
		TEST_COLLECTION_RELEASE(dataSet->nodes, item);
	}

	/**
	 * Reads the node again, in the way a walk reads it, and returns true if
	 * every route out of it leads back to it.
	 */
	static bool pointsAtItself(
		fiftyoneDegreesDataSetHash *dataSet,
		uint32_t offset,
		fiftyoneDegreesException *exception) {
		fiftyoneDegreesCollectionItem item;
		fiftyoneDegreesDataReset(&item.data);
		fiftyoneDegreesGraphNode * const node = fiftyoneDegreesGraphGetNode(
			dataSet->nodes,
			offset,
			&item,
			exception);
		if (node == NULL) {
			return false;
		}
		const fiftyoneDegreesGraphNodeHash * const hashes =
			(const fiftyoneDegreesGraphNodeHash*)(node + 1);
		bool result = node->unmatchedNodeOffset == (int32_t)offset;
		for (int32_t i = 0; i < node->hashesCount; i++) {
			if (isRoute(node, i) && hashes[i].nodeOffset != (int32_t)offset) {
				result = false;
			}
		}
		TEST_COLLECTION_RELEASE(dataSet->nodes, item);
		return result;
	}
};

/*
 * Loads the data file in memory, where the nodes can be changed, makes the
 * root of every graph that a detection from a User-Agent starts at lead back
 * to itself, checks that it does, and then detects a User-Agent. The first
 * step of a walk returns to the root, so the walk meets the cycle whatever it
 * matches. The detection runs on another thread so that a walk which never
 * ends is reported as a failure after a time limit, rather than holding the
 * test run. A thread that never ends cannot be stopped, so the process exits
 * with a failure in that case.
 */
TEST_F(HashGraphCycleTests, CycleEndsWithCorruptData)
{
	if (dataFilePath.empty()) {
		GTEST_SKIP() << "The Lite Hash data file was not found.";
	}
	EXCEPTION_CREATE;
	fiftyoneDegreesResourceManager manager;
	fiftyoneDegreesConfigHash config = fiftyoneDegreesHashInMemoryConfig;
	// With no difference or drift a walk follows only exact matches and the
	// unmatched route, which are the routes the test changes.
	config.difference = 0;
	config.drift = 0;
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
	// Frees the manager however the test ends, so a failed assertion before
	// the detection is not also reported as a leak.
	struct ManagerGuard {
		fiftyoneDegreesResourceManager *manager;
		~ManagerGuard() { fiftyoneDegreesResourceManagerFree(manager); }
	} managerGuard = { &manager };

	fiftyoneDegreesDataSetHash * const dataSet =
		fiftyoneDegreesDataSetHashGet(&manager);
	// In memory, the nodes a walk reads are the nodes changed here.
	const bool isInMemory = dataSet->b.b.isInMemory;
	vector<uint32_t> roots;
	addUserAgentRoots(dataSet, roots, exception);
	// A route to an offset of zero or less ends the walk at a leaf, so a
	// root at offset zero cannot lead back to itself.
	bool rootsAfterZero = true;
	for (const uint32_t offset : roots) {
		rootsAfterZero = rootsAfterZero && offset > 0;
	}
	if (isInMemory && rootsAfterZero && HasFatalFailure() == false) {
		for (const uint32_t offset : roots) {
			pointAtItself(dataSet, offset, exception);
			if (HasFatalFailure()) {
				break;
			}
		}
	}
	bool everyRootIsACycle = roots.empty() == false;
	for (const uint32_t offset : roots) {
		everyRootIsACycle = everyRootIsACycle &&
			pointsAtItself(dataSet, offset, exception);
	}
	fiftyoneDegreesDataSetHashRelease(dataSet);
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(isInMemory) << "The data set must be in memory.";
	ASSERT_FALSE(roots.empty()) <<
		"The data file has no graph for the User-Agent.";
	ASSERT_TRUE(rootsAfterZero) <<
		"A graph for the User-Agent starts at offset zero.";
	ASSERT_TRUE(everyRootIsACycle) <<
		"Every graph for the User-Agent must start at a node which leads "
		"back to itself, or the walk meets no cycle and the test shows "
		"nothing.";

	fiftyoneDegreesResultsHash * const results =
		fiftyoneDegreesResultsHashCreate(&manager, 0, 0);
	const string userAgent =
		"Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) "
		"AppleWebKit/537.51.2 (KHTML, like Gecko) Version/7.0 Mobile/11D167 "
		"Safari/9537.53";
	// The status starts as not set, as the library treats any other status,
	// success included, as an exception already raised and does nothing.
	fiftyoneDegreesException detectionException;
	detectionException.file = NULL;
	detectionException.func = NULL;
	detectionException.line = -1;
	detectionException.status = FIFTYONE_DEGREES_STATUS_NOT_SET;
	// With the properties named, the graphs are walked as the evidence is
	// added. The device id is read as well, which walks any graph left for
	// the first read.
	char deviceId[256];
	auto detection = std::async(std::launch::async, [&]() {
		fiftyoneDegreesResultsHashFromUserAgent(
			results,
			userAgent.c_str(),
			userAgent.length(),
			&detectionException);
		if (detectionException.status == FIFTYONE_DEGREES_STATUS_NOT_SET) {
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
}
