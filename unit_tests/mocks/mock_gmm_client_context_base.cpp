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
    case GMM_RESOURCE_USAGE_OCL_BUFFER:
        retVal.DwordValue = 6u;
        break;
    case GMM_RESOURCE_USAGE_OCL_BUFFER_CONST:
        retVal.DwordValue = 5u;
        break;
    case GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED:
        retVal.DwordValue = 0u;
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
} // namespace NEO
