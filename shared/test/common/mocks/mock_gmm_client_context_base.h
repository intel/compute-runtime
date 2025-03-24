/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"

namespace NEO {
class MockGmmClientContextBase : public GmmClientContext {
  public:
    struct MockPatIndex {
        static constexpr uint32_t uncached = 1;
        static constexpr uint32_t cached = 2;
        static constexpr uint32_t twoWayCoherent = 3;
        static constexpr uint32_t error = GMM_PAT_ERROR;
    };

    MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) override;
    uint32_t cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage, bool compressed, bool cachable) override;
    GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) override;
    GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) override;
    void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) override;
    long deallocate2(DeallocateGmm *deallocateGmm) override;
    uint8_t getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) override;
    uint8_t getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) override;
    void setGmmDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override;
    uint64_t mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) override;
    uint64_t freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pMapGpuVa) override;

    GMM_RESOURCE_FORMAT capturedFormat = GMM_FORMAT_INVALID;
    uint8_t compressionFormatToReturn = 1;
    uint32_t getSurfaceStateCompressionFormatCalled = 0u;
    uint32_t getMediaSurfaceStateCompressionFormatCalled = 0u;
    uint32_t mapGpuVirtualAddressCalled = 0u;
    uint32_t freeGpuVirtualAddressCalled = 0u;
    bool returnErrorOnPatIndexQuery = false;

    bool passedCompressedSettingForGetPatIndexQuery = false;
    bool passedCachableSettingForGetPatIndexQuery = false;

  protected:
    using GmmClientContext::GmmClientContext;
};
} // namespace NEO
