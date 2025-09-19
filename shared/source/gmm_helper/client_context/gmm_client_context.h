/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

#include <memory>

namespace NEO {
class GmmClientContext;
struct RootDeviceEnvironment;
class GmmHandleAllocator;
class MapGpuVirtualAddressGmm;
class FreeGpuVirtualAddressGmm;
class DeallocateGmm;

class GmmClientContext {
  public:
    GmmClientContext(const RootDeviceEnvironment &rootDeviceEnvironment);
    MOCKABLE_VIRTUAL ~GmmClientContext();

    MOCKABLE_VIRTUAL MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage);
    MOCKABLE_VIRTUAL uint32_t cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage, bool compressed, bool cacheable);

    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams);
    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes);
    MOCKABLE_VIRTUAL void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo);
    MOCKABLE_VIRTUAL long deallocate2(DeallocateGmm *deallocateGmm);
    MOCKABLE_VIRTUAL uint64_t mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa);
    MOCKABLE_VIRTUAL uint64_t freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa);
    GMM_CLIENT_CONTEXT *getHandle() const;
    template <typename T>
    static std::unique_ptr<GmmClientContext> create(const RootDeviceEnvironment &rootDeviceEnvironment) {
        return std::make_unique<T>(rootDeviceEnvironment);
    }

    MOCKABLE_VIRTUAL uint8_t getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);
    MOCKABLE_VIRTUAL uint8_t getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);

    void setHandleAllocator(std::unique_ptr<GmmHandleAllocator> allocator);

    MOCKABLE_VIRTUAL void setGmmDeviceInfo(GMM_DEVICE_INFO *deviceInfo);

    GmmHandleAllocator *getHandleAllocator() {
        return handleAllocator.get();
    }

  protected:
    GMM_CLIENT_CONTEXT *clientContext;
    std::unique_ptr<GmmHandleAllocator> handleAllocator;
};
} // namespace NEO
