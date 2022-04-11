/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_client_context.h"

namespace NEO {
MockGmmClientContext::MockGmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo) : MockGmmClientContextBase(osInterface, hwInfo) {
}

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
        retVal.DwordValue = 0u;
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

uint32_t MockGmmClientContextBase::cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage) {
    if (returnErrorOnPatIndexQuery) {
        return MockPatIndex::error;
    }

    if (usage == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) {
        return MockPatIndex::uncached;
    }

    return MockPatIndex::cached;
}

} // namespace NEO
