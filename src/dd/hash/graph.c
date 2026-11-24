/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is the subject of the following patents and patent
 * applications, owned by 51 Degrees Mobile Experts Limited of 5 Charlotte
 * Close, Caversham, Reading, Berkshire, United Kingdom RG4 7BY:
 * European Patent No. 3438848; and
 * United States Patent No. 10,482,175.
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

#include "graph.h"
#include "fiftyone.h"
#include <stdarg.h>

#ifndef FIFTYONE_DEGREES_MEMORY_ONLY

static uint32_t getNodeFinalSize(
	const void * const initial,
	Exception * const exception) {
	(void)exception;
	GraphNode *nodeHeader =
		(GraphNode*)initial;
	size_t size = sizeof(GraphNode);
	if (nodeHeader->hashesCount > 0) {
		size += sizeof(GraphNodeHash) *
			nodeHeader->hashesCount;
	}
	return (uint32_t)size;
}

static const CollectionKeyType nodeKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(GraphNode),
	getNodeFinalSize,
};

void* fiftyoneDegreesGraphNodeReadFromFile(
	const fiftyoneDegreesCollectionFile * const file,
	const CollectionKey * const key,
	fiftyoneDegreesData * const data,
	fiftyoneDegreesException * const exception) {
	GraphNode nodeHeader;
	return CollectionReadFileVariable(
		file,
		data,
		key,
		&nodeHeader,
		exception);
}

#else

static const CollectionKeyType nodeKeyType = {
	FIFTYONE_DEGREES_COLLECTION_ENTRY_TYPE_CUSTOM,
	sizeof(GraphNode),
	NULL,
};

#endif

fiftyoneDegreesGraphNode* fiftyoneDegreesGraphGetNode(
	fiftyoneDegreesCollection *collection,
	uint32_t offset,
	fiftyoneDegreesCollectionItem *item,
	fiftyoneDegreesException *exception) {
	const CollectionKey nodeKey = {
		{offset},
		&nodeKeyType,
	};
	return (GraphNode*)collection->get(
		collection,
		&nodeKey,
		item,
		exception);
}

fiftyoneDegreesGraphNodeHash*
fiftyoneDegreesGraphGetMatchingHashFromListNodeTable(
	fiftyoneDegreesGraphNode *node,
	uint32_t hash) {
	fiftyoneDegreesGraphNodeHash *foundHash = NULL;
	fiftyoneDegreesGraphNodeHash *nodeHashes, *nodeHash, *bucketEnd;
	uint32_t index;
	if (hash == 0) {
		// A hash code of zero is reserved as a marker in this layout: it marks
		// a slot as empty or as the head of a collision bucket, and terminates
		// a bucket. Comparing a hash of zero against the records would match
		// one of those markers rather than a stored hash code, so return no
		// match before any comparison is made.
		return NULL;
	}
	nodeHashes = (GraphNodeHash*)(node + 1);
	// The modulo is a positive value no greater than the number of hashes, so
	// the index is always that of a record of the node.
	index = hash % (uint32_t)node->modulo;
	nodeHash = &nodeHashes[index];
	if (hash == nodeHash->hashCode) {
		// There is a single record at this index and it matched, so return it.
		foundHash = nodeHash;
	}
	else if (nodeHash->hashCode == 0 &&
		nodeHash->nodeOffset > 0 &&
		nodeHash->nodeOffset < node->hashesCount) {
		// There are multiple records at this index, so go through them to find
		// a match. The bucket is terminated by a record with a hash code of
		// zero, but stop at the end of the records in case the terminator is
		// missing from the data file.
		bucketEnd = &nodeHashes[node->hashesCount];
		nodeHash = &nodeHashes[nodeHash->nodeOffset];
		while (nodeHash < bucketEnd && nodeHash->hashCode != 0) {
			if (hash == nodeHash->hashCode) {
				// There was a match, so stop looking.
				foundHash = nodeHash;
				break;
			}
			nodeHash++;
		}
	}
	return foundHash;
}

fiftyoneDegreesGraphNodeHash*
fiftyoneDegreesGraphGetMatchingHashFromListNodeSearch(
	fiftyoneDegreesGraphNode *node,
	uint32_t hash) {
	GraphNodeHash* foundHash = NULL;
	GraphNodeHash* nodeHashes = (GraphNodeHash*)(node + 1);
	int32_t lower = 0, upper = node->hashesCount - 1, middle;
	while (lower <= upper) {
		middle = lower + (upper - lower) / 2;
		if (nodeHashes[middle].hashCode == hash) {
			foundHash = &nodeHashes[middle];
			break;
		}
		else if (nodeHashes[middle].hashCode > hash) {
			upper = middle - 1;
		}
		else {
			lower = middle + 1;
		}
	}
	return foundHash;
}

fiftyoneDegreesGraphNodeHash* 
fiftyoneDegreesGraphGetMatchingHashFromListNode(
	fiftyoneDegreesGraphNode *node,
	uint32_t hash) {
	fiftyoneDegreesGraphNodeHash *foundHash;
	if (node->modulo == 0) {
		foundHash = GraphGetMatchingHashFromListNodeSearch(
			node,
			hash);
	}
	else if (GRAPH_NODE_IS_HASH_TABLE(node)) {
		foundHash = GraphGetMatchingHashFromListNodeTable(
			node,
			hash);
	}
	else {
		// The modulo is negative, or greater than the number of hashes, so the
		// records cannot be indexed as a hash table. Rather than read beyond
		// the records of the node, report that nothing matched so that the
		// unmatched branch of the graph is taken.
		foundHash = NULL;
	}
	return foundHash;
}

fiftyoneDegreesGraphNodeHash*
fiftyoneDegreesGraphGetMatchingHashFromBinaryNode(
	fiftyoneDegreesGraphNode *node,
	uint32_t hash) {
	GraphNodeHash *nodeHash = (GraphNodeHash*)(node + 1);
	if (nodeHash->hashCode == hash) {
		return nodeHash;
	}
	else {
		return NULL;
	}
}

fiftyoneDegreesGraphNodeHash*
fiftyoneDegreesGraphGetMatchingHashFromNode(
	fiftyoneDegreesGraphNode *node,
	uint32_t hash) {
	if (node->hashesCount == 1) {
		return GraphGetMatchingHashFromBinaryNode(node, hash);
	}
	else {
		return GraphGetMatchingHashFromListNode(node, hash);
	}
}

fiftyoneDegreesGraphTraceNode* fiftyoneDegreesGraphTraceCreate(
	const char* fmt,
	...) {
	size_t length;
	GraphTraceNode* root = (GraphTraceNode*)Malloc(sizeof(GraphTraceNode));
	if (fmt != NULL) {
		va_list args;
		va_start(args, fmt);
		va_list args2;
		va_copy(args2, args);

		// state of args is undetermined after the following statement
		// so end the args and use the args2 when perform the actual construction
		length = vsnprintf(NULL, 0, fmt, args);
		va_end(args);
		root->rootName = (char*)Malloc((length + 1) * sizeof(char));
		vsnprintf(root->rootName, length + 1, fmt, args2);
		va_end(args2);
	}
	else {
		root->rootName = NULL;
	}
	
	root->hashCode = 0;
	root->index = 0;
	root->length = 0;
	root->matched = false;
	root->next = NULL;
	return root;
}


void fiftyoneDegreesGraphTraceFree(fiftyoneDegreesGraphTraceNode* route) {
	GraphTraceNode *tmp, *current = route;
	while (current != NULL) {
		tmp = current->next;
		if (current->rootName != NULL) {
			Free(current->rootName);
		}
		Free(current);
		current = tmp;
	}
}

void fiftyoneDegreesGraphTraceAppend(
	fiftyoneDegreesGraphTraceNode* route,
	fiftyoneDegreesGraphTraceNode* node) {
	GraphTraceNode *last = route;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = node;
}

/*
 * Obtain the remaining length of the buffer available for writing
 * @param length of the buffer
 * @param written buffer that have or might have been used
 * @return the remaining length of the buffer, available for writing
 */
static size_t getRemainingBuffer(size_t length, size_t written) {
	if (length == 0 || length <= written) {
		return 0;
	}
	else {
		return length - written;
	}
}

#define CURRENT(s,w) s == NULL ? NULL : s + w

int fiftyoneDegreesGraphTraceGet(
	char *destination,
	size_t length,
	fiftyoneDegreesGraphTraceNode* route,
	const char *source) {
	int written = 0, tmpWritten = 0;
	size_t remaining = 0;
	uint32_t i;
	GraphTraceNode *node = route;

	while (node != NULL) {
		if (node->rootName != NULL) {
			tmpWritten = Snprintf(
				CURRENT(destination, written),
				getRemainingBuffer(length, written),
				"--- Start of '%s'---\n",
				node->rootName);
			if (tmpWritten < 0) {
				// Something has gone wrong
				written = tmpWritten;
				break;
			}
			written += tmpWritten;
		}
		else {
			for (i = 0; i < node->lastIndex + node->length; i++) {
				if (getRemainingBuffer(length, written) > 0) {
					if (i < node->firstIndex) {
						(destination + written)[0] = ' ';
					}
					else if (i >= node->index &&
						i < node->index + node->length) {
						(destination + written)[0] =
							(source == NULL || node->matched == false) ?
							'^' : source[i];
					}
					else if (i == node->firstIndex || i == node->lastIndex + node->length - 1) {
						(destination + written)[0] = '|';
					}
					else {
						(destination + written)[0] = '-';
					}
				}
				written++;
			}

			remaining = getRemainingBuffer(length, written);
			tmpWritten = node->matched ?
				Snprintf(
					CURRENT(destination, written),
					remaining,
					"(%d) %x\n",
					node->index,
					node->hashCode) :
				Snprintf(
					CURRENT(destination, written),
					remaining,
					"(%d)\n",
					node->index);

			if (tmpWritten < 0) {
				// Something has gone wrong
				// Set to negative value to indicate error
				written = tmpWritten;
				break;
			}
			written += tmpWritten;
		}
		node = node->next;
	}
	return written;
}
