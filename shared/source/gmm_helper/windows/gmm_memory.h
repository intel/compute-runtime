/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace NEO {
class GmmClientContext;
class GmmMemory {
  public:
    static GmmMemory *create(GmmClientContext *gmmClientContext);
    virtual ~GmmMemory() = default;

    MOCKABLE_VIRTUAL bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                      GMM_ESCAPE_HANDLE hDevice,
                                                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                      GMM_GFX_SIZE_T svmSize,
                                                      BOOLEAN bdwL3Coherency);

    bool configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                         GMM_ESCAPE_HANDLE hDevice,
                         GMM_ESCAPE_FUNC_TYPE pfnEscape,
                         GMM_GFX_SIZE_T svmSize,
                         BOOLEAN bdwL3Coherency,
                         uintptr_t &minAddress,
                         bool obtainMinAddress);

    MOCKABLE_VIRTUAL uintptr_t getInternalGpuVaRangeLimit();

    MOCKABLE_VIRTUAL bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo);

  protected:
    GmmMemory(GmmClientContext *gmmClientContext);
    GMM_CLIENT_CONTEXT &clientContext;
};
} // namespace NEO
