/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

#include "gtest/gtest.h"

namespace NEO {
GMM_RESOURCE_INFO *MockGmmClientContextBase::createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) {
    return reinterpret_cast<GMM_RESOURCE_INFO *>(new char[sizeof(GMM_RESOURCE_INFO)]);
}

GMM_RESOURCE_INFO *MockGmmClientContextBase::copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) {
    return reinterpret_cast<GMM_RESOURCE_INFO *>(new char[sizeof(GMM_RESOURCE_INFO)]);
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

uint64_t MockGmmClientContextBase::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    freeGpuVirtualAddressCalled++;
    return 0;
}

void MockGmmClientContextBase::initialize(const RootDeviceEnvironment &rootDeviceEnvironment) {
    initializeCalled++;
    clientContext = {reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x08), [](auto) {}};
};
} // namespace NEO
