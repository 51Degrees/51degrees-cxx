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

//
//  HeadersContainer.cpp
//  CommonTests
//
//  Created by Eugene Dorfman on 8/6/24.
//

#include "HeadersContainer.hpp"
#include "../fiftyone.h"

void HeadersContainer::CreateHeaders(
    const char** headersList,
    int headersCount,
    bool expectUpperPrefixedHeaders) {
    EXCEPTION_CREATE
    count = headersCount;
    strings = new StringCollection(headersList, count);
    headers = fiftyoneDegreesHeadersCreate(
        expectUpperPrefixedHeaders,
        strings->getState(),
        getHeaderUniqueId,
        exception);
    
    FIFTYONE_DEGREES_ARRAY_CREATE(fiftyoneDegreesHeaderPtr, headerPointers, headersCount);
    if (headerPointers) {
        for (int i=0;i<headersCount;++i) {
            headerPointers->items[i] = &headers->items[i];
            headerPointers->count++;
        }
    }
}

void HeadersContainer::Dealloc() {
    if (headers != nullptr) {
        fiftyoneDegreesHeadersFree(headers);
        headers = nullptr;
    }
    if (strings != nullptr) {
        delete strings;
        strings = nullptr;
    }
    if (headerPointers != nullptr) {
        fiftyoneDegreesFree(headerPointers);
        headerPointers = nullptr;
    }
}
