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

#include "EvidenceBase.hpp"

#include "fiftyone.h"

using namespace FiftyoneDegrees::Common;

EvidenceBase::EvidenceBase() {
	evidence = NULL;
}

EvidenceBase::~EvidenceBase() {
	if (evidence != NULL) {
		EvidenceFree(evidence);
		evidence = NULL;
	}
}

fiftyoneDegreesEvidenceKeyValuePairArray* EvidenceBase::get() {
	if (evidence != NULL) {
		EvidenceFree(evidence);
		evidence = NULL;
	}
	evidence = EvidenceCreate((uint32_t)size());
	if (evidence != NULL) {
		for (map<string, string>::const_iterator iterator = begin();
			iterator != end();
			iterator++) {
			// Keys are case-insensitive, so a key that differs only in case
			// from one earlier in the map is the same key, and is left out.
			bool duplicate = false;
			for (std::map<string, string>::const_iterator earlier = begin();
				earlier != iterator;
				earlier++) {
				if (StringCompare(
					earlier->first.c_str(),
					iterator->first.c_str()) == 0) {
					duplicate = true;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			EvidencePrefixMap* map =
				EvidenceMapPrefix(iterator->first.c_str());
			if (map != NULL && isRelevant(map->prefixEnum)) {
				EvidenceAddString(
					evidence,
					map->prefixEnum,
					iterator->first.c_str() + map->prefixLength,
					iterator->second.c_str());
			}
		}
	}
	return evidence;
}

#ifdef _MSC_VER
#pragma warning (disable:4100)  
#endif
bool EvidenceBase::isRelevant(
	fiftyoneDegreesEvidencePrefix prefix) {
	return true;
}
#ifdef _MSC_VER
#pragma warning (default:4100)  
#endif

EvidenceBase::iterator EvidenceBase::findIgnoringCase(const string& key) {
	iterator item = find(key);
	if (item != end()) {
		return item;
	}
	for (item = begin(); item != end(); item++) {
		if (StringCompare(item->first.c_str(), key.c_str()) == 0) {
			return item;
		}
	}
	return end();
}

string& EvidenceBase::operator[](const string& key) {
	iterator item = findIgnoringCase(key);
	if (item != end()) {
		return item->second;
	}
	return map<string, string>::operator[](key);
}

string& EvidenceBase::operator[](string&& key) {
	iterator item = findIgnoringCase(key);
	if (item != end()) {
		return item->second;
	}
	return map<string, string>::operator[](std::move(key));
}

void EvidenceBase::clear() {
	map<string, string>::clear();
	EvidenceFree(evidence);
	evidence = NULL;
}

void EvidenceBase::erase(iterator position) {
	map<string, string>::erase(position);
	EvidenceFree(evidence);
	evidence = NULL;
}

void EvidenceBase::erase(iterator first, iterator last) {
	map<string, string>::erase(first, last);
	EvidenceFree(evidence);
	evidence = NULL;
}