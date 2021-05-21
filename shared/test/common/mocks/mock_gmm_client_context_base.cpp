/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

namespace NEO {

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
