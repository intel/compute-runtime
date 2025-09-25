/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_client_context.h"

namespace NEO {

MEMORY_OBJECT_CONTROL_STATE MockGmmClientContextBase::cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) {
    MEMORY_OBJECT_CONTROL_STATE retVal = {};
    memset(&retVal, 0, sizeof(MEMORY_OBJECT_CONTROL_STATE));
    switch (usage) {
    case GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC:
        retVal.DwordValue = 32u;
        break;
    case GMM_RESOURCE_USAGE_OCL_BUFFER:
        retVal.DwordValue = 16u;
        break;
    case GMM_RESOURCE_USAGE_OCL_BUFFER_CONST:
        retVal.DwordValue = 8u;
        break;
    case GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED:
        retVal.DwordValue = 6u;
        break;
    case GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER:
        retVal.DwordValue = 2u;
        break;
    default:
        retVal.DwordValue = 4u;
        break;
    }
    return retVal;
}

uint32_t MockGmmClientContextBase::cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage, bool compressed, bool cacheable) {
    passedCompressedSettingForGetPatIndexQuery = compressed;
    passedCacheableSettingForGetPatIndexQuery = cacheable;

    if (returnErrorOnPatIndexQuery) {
        return MockPatIndex::error;
    }

    if (usage == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED ||
        usage == GMM_RESOURCE_USAGE_SURFACE_UNCACHED) {
        return MockPatIndex::uncached;
    }

    if (usage == GMM_RESOURCE_USAGE_HW_CONTEXT) {
        return MockPatIndex::twoWayCoherent;
    }

    return MockPatIndex::cached;
}

} // namespace NEO
