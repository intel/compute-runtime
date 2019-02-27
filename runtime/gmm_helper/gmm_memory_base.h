/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"

#include <memory>

namespace OCLRT {
class GmmMemoryBase {
  public:
    virtual ~GmmMemoryBase() = default;

    MOCKABLE_VIRTUAL bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                      GMM_ESCAPE_HANDLE hDevice,
                                                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                      GMM_GFX_SIZE_T SvmSize,
                                                      BOOLEAN BDWL3Coherency);

    virtual bool configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                                 GMM_ESCAPE_HANDLE hDevice,
                                 GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                 GMM_GFX_SIZE_T SvmSize,
                                 BOOLEAN BDWL3Coherency,
                                 GMM_GFX_PARTITIONING &gfxPartition,
                                 uintptr_t &minAddress);

  protected:
    GmmMemoryBase();
    GMM_CLIENT_CONTEXT *clientContext = nullptr;
};
} // namespace OCLRT
