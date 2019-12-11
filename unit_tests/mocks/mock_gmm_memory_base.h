/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/windows/windows_defs.h"

#include "gmm_memory.h"
#include "gmock/gmock.h"

namespace NEO {

class MockGmmMemoryBase : public GmmMemory {
  public:
    ~MockGmmMemoryBase() = default;

    MockGmmMemoryBase() = default;

    bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                     GMM_ESCAPE_HANDLE hDevice,
                                     GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                     GMM_GFX_SIZE_T SvmSize,
                                     BOOLEAN BDWL3Coherency) override {
        return true;
    }
};

class GmockGmmMemoryBase : public GmmMemory {
  public:
    ~GmockGmmMemoryBase() = default;

    GmockGmmMemoryBase() = default;

    MOCK_METHOD5(configureDeviceAddressSpace,
                 bool(GMM_ESCAPE_HANDLE hAdapter,
                      GMM_ESCAPE_HANDLE hDevice,
                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                      GMM_GFX_SIZE_T SvmSize,
                      BOOLEAN BDWL3Coherency));
};
} // namespace NEO
