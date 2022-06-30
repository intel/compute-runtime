/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

#include "gtest/gtest.h"

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

void MockGmmClientContextBase::setGmmDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    EXPECT_NE(deviceInfo, nullptr);

    GMM_DEVICE_CALLBACKS_INT emptyStruct{};
    EXPECT_EQ(0, memcmp(deviceInfo->pDeviceCb, &emptyStruct, sizeof(GMM_DEVICE_CALLBACKS_INT)));
}
} // namespace NEO
