/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/gmm_lib.h"

#include <memory>

namespace NEO {
class GmmClientContext;
class OSInterface;
struct HardwareInfo;

class GmmClientContext {
  public:
    GmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmClientContext();

    MOCKABLE_VIRTUAL MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage);
    MOCKABLE_VIRTUAL uint32_t cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage);

    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams);
    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes);
    MOCKABLE_VIRTUAL void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo);
    GMM_CLIENT_CONTEXT *getHandle() const;
    template <typename T>
    static std::unique_ptr<GmmClientContext> create(OSInterface *osInterface, HardwareInfo *hwInfo) {
        return std::make_unique<T>(osInterface, hwInfo);
    }

    const HardwareInfo *getHardwareInfo() {
        return hardwareInfo;
    }

    MOCKABLE_VIRTUAL uint8_t getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);
    MOCKABLE_VIRTUAL uint8_t getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);

    void setHandleAllocator(std::unique_ptr<GmmHandleAllocator> allocator) {
        this->handleAllocator = std::move(allocator);
    }

    MOCKABLE_VIRTUAL void setGmmDeviceInfo(GMM_DEVICE_INFO *deviceInfo);

    GmmHandleAllocator *getHandleAllocator() {
        return handleAllocator.get();
    }

  protected:
    HardwareInfo *hardwareInfo = nullptr;
    GMM_CLIENT_CONTEXT *clientContext;
    std::unique_ptr<GmmHandleAllocator> handleAllocator;
};
} // namespace NEO
