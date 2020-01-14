/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"

#include <memory>

namespace NEO {
class GmmClientContext;
class OSInterface;
struct HardwareInfo;

class GmmClientContextBase {
  public:
    virtual ~GmmClientContextBase();

    MOCKABLE_VIRTUAL MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage);
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

  protected:
    HardwareInfo *hardwareInfo = nullptr;
    GMM_CLIENT_CONTEXT *clientContext;
    GmmClientContextBase(OSInterface *osInterface, HardwareInfo *hwInfo);
};
} // namespace NEO
