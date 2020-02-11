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
class GmmMemoryBase {
  public:
    virtual ~GmmMemoryBase() = default;

    MOCKABLE_VIRTUAL bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                      GMM_ESCAPE_HANDLE hDevice,
                                                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                      GMM_GFX_SIZE_T SvmSize,
                                                      BOOLEAN BDWL3Coherency);

    bool configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                         GMM_ESCAPE_HANDLE hDevice,
                         GMM_ESCAPE_FUNC_TYPE pfnEscape,
                         GMM_GFX_SIZE_T SvmSize,
                         BOOLEAN BDWL3Coherency,
                         uintptr_t &minAddress,
                         bool obtainMinAddress);

    MOCKABLE_VIRTUAL uintptr_t getInternalGpuVaRangeLimit();

    MOCKABLE_VIRTUAL bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo);

  protected:
    GmmMemoryBase(GmmClientContext *gmmClientContext);
    GMM_CLIENT_CONTEXT &clientContext;
};
} // namespace NEO
