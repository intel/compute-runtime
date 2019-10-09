/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_gmm_client_context.h"

namespace NEO {
MockGmmClientContextBase::MockGmmClientContextBase(GMM_CLIENT clientType, GmmExportEntries &gmmExportEntries) : GmmClientContext(clientType, gmmExportEntries) {
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

GMM_RESOURCE_INFO *MockGmmClientContextBase::createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) {
    return reinterpret_cast<GMM_RESOURCE_INFO *>(new char[1]);
}

GMM_RESOURCE_INFO *MockGmmClientContextBase::copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) {
    return reinterpret_cast<GMM_RESOURCE_INFO *>(new char[1]);
}

void MockGmmClientContextBase::destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) {
    delete[] reinterpret_cast<char *>(pResInfo);
}

uint8_t MockGmmClientContextBase::getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    capturedFormat = format;
    getSurfaceStateCompressionFormatCalled++;
    return compressionFormatToReturn;
}

uint8_t MockGmmClientContextBase::getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    capturedFormat = format;
    getMediaSurfaceStateCompressionFormatCalled++;
    return compressionFormatToReturn;
}

} // namespace NEO
