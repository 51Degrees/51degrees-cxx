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
 * @file IpiGraphLeafTests.cpp
 * @brief Walks the leaves of every graph in key order, reading the graph from
 * the data file rather than through the library, and checks that graph
 * evaluation agrees with the walk.
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Constants.hpp"
#include "../common/Base.hpp"
#include "../../src/ipi/EngineIpi.hpp"
#include "../../src/ipi/fiftyone.h"

using namespace std;

// Committed to the data submodule, so the ASN file is checked in every build.
static const char asnFileName[] = "51Degrees-IPIV4AsnIpiV41.ipi";
static const char enterpriseFileName[] = "51Degrees-EnterpriseIpiV41.ipi";

/**
 * Which keys of a graph are checked. Every key when keysPerWindow is zero.
 * Otherwise the first keysPerWindow keys at or after the start of each of
 * 2^bits equal parts of the address space, with bits for the graph's IP
 * version.
 */
struct Sample {
	uint32_t ipv4Bits;
	uint32_t ipv6Bits;
	uint32_t keysPerWindow;
};
static const Sample everyKey = { 0, 0, 0 };

// The Enterprise file has over a hundred million keys, so each of its graphs
// is sampled. IPv6 uses more parts because its ranges are spread more thinly.
static const Sample enterpriseKeys = { 10, 20, 16 };

// Failures reported in full for each graph. The rest are counted.
static const uint64_t reportedFailures = 10;

// An address with the most significant byte first. IPv4 uses the first four.
typedef array<uint8_t, FIFTYONE_DEGREES_IPV6_LENGTH> Key;

/**
 * One graph read from the data file, so that the walk does not depend on the
 * code it checks.
 */
class FileGraph {
public:
	FileGraph(ifstream& file, const fiftyoneDegreesIpiCgInfo& graphInfo)
		: info(graphInfo),
		nodes(read(file, graphInfo.nodes.collection)),
		spans(read(file, graphInfo.spans)),
		spanBytes(read(file, graphInfo.spanBytes)),
		clusters(read(file, graphInfo.clusters)) {
		if (nodes.empty() || spans.empty() || clusters.empty() ||
			info.spans.count == 0 || info.clusters.count == 0) {
			throw runtime_error("The graph could not be read from the file");
		}
	}

	/**
	 * Calls visit with the key and leaf of every leaf in key order, skipping
	 * the parts below a node for which keep returns false. Nodes can be shared
	 * by several ranges, so a node is visited once for each range below it.
	 * Throws if the graph has a cycle or does not fit the sizes in its header.
	 */
	template <typename K, typename V> void walk(K keep, V visit) const {
		const uint32_t width = info.version == 4 ? 32 : 128;
		const uint32_t count = info.nodes.collection.count;
		vector<Step> stack;
		stack.push_back({ info.graphIndex, false, 0, 0, Key{} });
		while (stack.empty() == false) {
			const Step step = stack.back();
			stack.pop_back();
			if (keep(step.key, step.bits) == false) {
				continue;
			}
			if (step.leaf) {
				visit(step.key, step.index);
				continue;
			}
			if (step.index >= count) {
				throw runtime_error("A node is outside the graph");
			}
			// A path with more nodes than the graph repeats one.
			if (step.depth >= count) {
				throw runtime_error("The graph has a cycle");
			}
			const uint64_t record = recordAt(step.index);
			const Span span = spanOf(step.index, record);
			if (step.bits + span.lowBits > width ||
				step.bits + span.highBits > width) {
				throw runtime_error("A key is longer than the address");
			}
			Step low = { step.index + 1, false, step.depth + 1,
				step.bits + span.lowBits, step.key };
			Step high = { 0, false, step.depth + 1,
				step.bits + span.highBits, step.key };
			copyBits(low.key, step.bits, *span.source, span.start,
				span.lowBits);
			copyBits(high.key, step.bits, *span.source,
				span.start + span.lowBits, span.highBits);
			uint64_t highValue = member(record, info.nodes.value);
			if (member(record, info.nodes.lowFlag) != 0) {
				// This record holds the low entry and the next the high one.
				point(low, highValue, count);
				highValue = member(recordAt(step.index + 1), info.nodes.value);
			}
			// Otherwise this record holds the high entry and the low entry is
			// the next node.
			point(high, highValue, count);
			stack.push_back(high);
			stack.push_back(low);
		}
	}

	const fiftyoneDegreesIpiCgInfo info;

private:
	struct Step {
		uint32_t index;
		bool leaf;
		uint32_t depth; // Nodes on the path so far
		uint32_t bits; // Bits of the key so far
		Key key;
	};

	struct Span {
		uint32_t lowBits;
		uint32_t highBits;
		const vector<uint8_t>* source;
		uint64_t start; // First bit of the low limit in the source
	};

	static vector<uint8_t> read(
		ifstream& file,
		const fiftyoneDegreesCollectionHeader& header) {
		vector<uint8_t> bytes(header.length);
		file.seekg((streamoff)header.startPosition);
		file.read((char*)bytes.data(), (streamsize)bytes.size());
		if (!file) {
			bytes.clear();
		}
		return bytes;
	}

	static uint32_t uint32At(const vector<uint8_t>& bytes, size_t offset) {
		return (uint32_t)bytes.at(offset) |
			((uint32_t)bytes.at(offset + 1) << 8) |
			((uint32_t)bytes.at(offset + 2) << 16) |
			((uint32_t)bytes.at(offset + 3) << 24);
	}

	// The count bits from bit start, the most significant first.
	static uint64_t bitsAt(
		const vector<uint8_t>& bytes,
		uint64_t start,
		uint32_t count) {
		uint64_t result = 0;
		while (count > 0) {
			const uint32_t offset = (uint32_t)(start % 8);
			const uint32_t take = 8 - offset < count ? 8 - offset : count;
			const uint32_t byte = bytes.at((size_t)(start / 8));
			result = (result << take) |
				((byte >> (8 - offset - take)) & ((1u << take) - 1));
			start += take;
			count -= take;
		}
		return result;
	}

	static void copyBits(
		Key& key,
		uint32_t to,
		const vector<uint8_t>& source,
		uint64_t from,
		uint32_t count) {
		for (uint32_t i = 0; i < count; i++) {
			if (bitsAt(source, from + i, 1) != 0) {
				key[(to + i) / 8] |= (uint8_t)(0x80 >> ((to + i) % 8));
			}
		}
	}

	static uint64_t member(
		uint64_t record,
		const fiftyoneDegreesIpiCgMember& part) {
		return (record & part.mask) >> part.shift;
	}

	// A value at or past the node count is a leaf.
	static void point(Step& step, uint64_t value, uint32_t count) {
		step.leaf = value >= count;
		step.index = (uint32_t)(step.leaf ? value - count : value);
	}

	uint64_t recordAt(uint32_t index) const {
		return bitsAt(
			nodes,
			(uint64_t)index * info.nodes.recordSize,
			info.nodes.recordSize);
	}

	// The clusters hold runs of nodes in order, and name the spans the nodes
	// in each run use.
	Span spanOf(uint32_t index, uint64_t record) const {
		const size_t clusterSize = info.clusters.length / info.clusters.count;
		uint32_t lower = 0;
		uint32_t upper = info.clusters.count - 1;
		while (lower < upper) {
			const uint32_t middle = lower + (upper - lower) / 2;
			if (uint32At(clusters, middle * clusterSize + 4) < index) {
				lower = middle + 1;
			}
			else {
				upper = middle;
			}
		}
		const size_t cluster = lower * clusterSize;
		const size_t slot = 8 + 4 * (size_t)member(record, info.nodes.spanIndex);
		if (index < uint32At(clusters, cluster) ||
			index > uint32At(clusters, cluster + 4) ||
			slot + 4 > clusterSize) {
			throw runtime_error("A node is outside the clusters");
		}
		const size_t spanSize = info.spans.length / info.spans.count;
		const size_t first = uint32At(clusters, cluster + slot) * spanSize;
		Span span;
		span.lowBits = spans.at(first);
		span.highBits = spans.at(first + 1);
		if (span.lowBits + span.highBits > 32) {
			span.source = &spanBytes;
			span.start = (uint64_t)uint32At(spans, first + 2) * 8;
		}
		else {
			span.source = &spans;
			span.start = (uint64_t)(first + 2) * 8;
		}
		return span;
	}

	const vector<uint8_t> nodes;
	const vector<uint8_t> spans;
	const vector<uint8_t> spanBytes;
	const vector<uint8_t> clusters;
};

class IpiGraphLeafTests : public Base {
protected:
	// Checks every graph in the file.
	void checkFile(const string& path, const Sample& sample) {
		fiftyoneDegreesConfigIpi config = fiftyoneDegreesIpiLowMemoryConfig;
		// Each graph in memory, so that the lookups are quick.
		config.graph.loaded = true;
		fiftyoneDegreesPropertiesRequired properties =
			fiftyoneDegreesPropertiesDefault;
		Manager manager(path, config, properties);
		DataSet dataSet(&manager.manager);
		ifstream file(path, ios::binary);
		ASSERT_TRUE(file.is_open()) << path;
		const fiftyoneDegreesIpiCgArray* graphs = dataSet.dataSet->graphsArray;
		ASSERT_GT(graphs->count, 0u);
		for (uint32_t i = 0; i < graphs->count; i++) {
			const FileGraph graph(file, graphs->items[i].info);
			checkGraph(graphs, graph, sample);
		}
	}

private:
	struct Manager {
		Manager(
			const string& path,
			fiftyoneDegreesConfigIpi& config,
			fiftyoneDegreesPropertiesRequired& properties) {
			EXCEPTION_CREATE;
			const fiftyoneDegreesStatusCode status =
				fiftyoneDegreesIpiInitManagerFromFile(
					&manager,
					&config,
					&properties,
					path.c_str(),
					exception);
			if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
				throw FiftyoneDegrees::Common::StatusCodeException(status);
			}
			EXCEPTION_THROW;
		}
		~Manager() {
			fiftyoneDegreesResourceManagerFree(&manager);
		}
		fiftyoneDegreesResourceManager manager;
	};

	struct DataSet {
		explicit DataSet(fiftyoneDegreesResourceManager* manager)
			: dataSet(fiftyoneDegreesDataSetIpiGet(manager)) {}
		~DataSet() {
			fiftyoneDegreesDataSetIpiRelease(dataSet);
		}
		fiftyoneDegreesDataSetIpi* dataSet;
	};

	static uint32_t bytesOf(const fiftyoneDegreesIpiCgInfo& info) {
		return info.version == 4 ?
			FIFTYONE_DEGREES_IPV4_LENGTH : FIFTYONE_DEGREES_IPV6_LENGTH;
	}

	static string text(const Key& key, uint32_t bytes) {
		stringstream out;
		for (uint32_t i = 0; i < bytes; i++) {
			if (bytes == FIFTYONE_DEGREES_IPV4_LENGTH) {
				out << (i > 0 ? "." : "") << (int)key[i];
			}
			else {
				out << (i > 0 && i % 2 == 0 ? ":" : "") << hex <<
					setw(2) << setfill('0') << (int)key[i];
			}
		}
		return out.str();
	}

	// The address one below the key.
	static Key before(Key key, uint32_t bytes) {
		for (uint32_t i = bytes; i > 0; i--) {
			if (key[i - 1]-- != 0) {
				break;
			}
		}
		return key;
	}

	// The leaf the library reaches for the key, or UINT32_MAX for none.
	static uint32_t evaluate(
		const fiftyoneDegreesIpiCgArray* graphs,
		const fiftyoneDegreesIpiCgInfo& info,
		const Key& key) {
		fiftyoneDegreesIpAddress address;
		memcpy(address.value, key.data(), sizeof(address.value));
		address.type = info.version;
		EXCEPTION_CREATE;
		const fiftyoneDegreesIpiCgResult result =
			fiftyoneDegreesIpiGraphEvaluate(
				graphs,
				info.componentId,
				address,
				exception);
		EXCEPTION_THROW;
		return result.rawOffset;
	}

	// The key with every bit from bits to width set.
	static Key lastOf(Key key, uint32_t bits, uint32_t width) {
		for (uint32_t i = bits; i < width; i++) {
			key[i / 8] |= (uint8_t)(0x80 >> (i % 8));
		}
		return key;
	}

	// Which of the 2^windowBits equal parts of the address space the key is
	// in, and the first address of a part.
	static uint64_t windowOf(const Key& key, uint32_t windowBits) {
		uint64_t index = 0;
		for (uint32_t i = 0; i < windowBits; i++) {
			index = (index << 1) | (uint64_t)((key[i / 8] >> (7 - i % 8)) & 1);
		}
		return index;
	}
	static Key windowStart(uint64_t index, uint32_t windowBits) {
		Key key{};
		for (uint32_t i = 0; i < windowBits; i++) {
			if ((index >> (windowBits - 1 - i)) & 1) {
				key[i / 8] |= (uint8_t)(0x80 >> (i % 8));
			}
		}
		return key;
	}

	static void checkGraph(
		const fiftyoneDegreesIpiCgArray* graphs,
		const FileGraph& graph,
		const Sample& sample) {
		const fiftyoneDegreesIpiCgInfo& info = graph.info;
		const uint32_t windowBits =
			info.version == 4 ? sample.ipv4Bits : sample.ipv6Bits;
		const uint32_t keysPerWindow = sample.keysPerWindow;
		const uint32_t bytes = bytesOf(info);
		const uint32_t width = bytes * 8;
		const string name = "IPv" + to_string(info.version) +
			" graph of component " + to_string(info.componentId);
		uint64_t checked = 0;
		uint64_t failures = 0;
		Key lowest{};
		Key previous{};
		uint32_t previousLeaf = 0;
		// True when the previous leaf checked is the one just before.
		bool adjacent = false;
		// True between the parts checked, until a key reaches target.
		bool skipping = false;
		bool finished = false;
		Key target{};
		uint32_t left = keysPerWindow;
		auto fail = [&](const string& message) {
			if (failures++ < reportedFailures) {
				ADD_FAILURE() << name << ": " << message;
			}
		};
		auto keep = [&](const Key& key, uint32_t bits) {
			return finished == false && (skipping == false ||
				(lastOf(key, bits, width) < target) == false);
		};
		graph.walk(keep, [&](const Key& key, uint32_t leaf) {
			if (skipping) {
				skipping = false;
				adjacent = false;
				left = keysPerWindow;
			}
			if (checked == 0) {
				lowest = key;
			}
			else if ((previous < key) == false) {
				fail(text(key, bytes) + " does not follow " +
					text(previous, bytes));
			}
			if (leaf >= info.profileCount + info.profileGroupCount) {
				fail(text(key, bytes) + " has leaf " + to_string(leaf) +
					", which names no profile or group");
			}
			const uint32_t found = evaluate(graphs, info, key);
			if (found != leaf) {
				fail(text(key, bytes) + " reaches leaf " + to_string(found) +
					" rather than " + to_string(leaf));
			}
			// The address just below a key is in the previous range.
			if (adjacent) {
				const Key below = before(key, bytes);
				const uint32_t belowFound = evaluate(graphs, info, below);
				if (belowFound != previousLeaf) {
					fail(text(below, bytes) + " reaches leaf " +
						to_string(belowFound) + " rather than " +
						to_string(previousLeaf));
				}
			}
			previous = key;
			previousLeaf = leaf;
			adjacent = true;
			checked++;
			if (keysPerWindow > 0 && --left == 0) {
				const uint64_t next = windowOf(key, windowBits) + 1;
				finished = (next >> windowBits) != 0;
				target = windowStart(next, windowBits);
				skipping = true;
			}
		});
		EXPECT_GT(checked, 0u) << name;
		EXPECT_EQ(0u, failures) << name << ": " << failures <<
			" failures in " << checked << " leaves";
		// No address is below a lowest key of zero.
		if (checked > 0 && lowest != Key{}) {
			const Key below = before(lowest, bytes);
			EXPECT_EQ(UINT32_MAX, evaluate(graphs, info, below)) << name <<
				": " << text(below, bytes) << ", below the lowest key, " <<
				"has a profile";
		}
	}
};

TEST_F(IpiGraphLeafTests, AsnFile) {
	const string path = GetFilePath(_dataFolderName, asnFileName);
	ASSERT_FALSE(path.empty()) << asnFileName << " not found.";
	checkFile(path, everyKey);
}

TEST_F(IpiGraphLeafTests, EnterpriseFile) {
	const string path = GetFilePath(_dataFolderName, enterpriseFileName);
	if (path.empty()) {
		GTEST_SKIP() << enterpriseFileName << " not found.";
	}
	checkFile(path, enterpriseKeys);
}
