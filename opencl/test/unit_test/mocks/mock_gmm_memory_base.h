/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_defs.h"

#include "gmm_memory.h"
#include "gmock/gmock.h"

namespace NEO {

class MockGmmMemoryBase : public GmmMemory {
  public:
    ~MockGmmMemoryBase() = default;

    MockGmmMemoryBase(GmmClientContext *gmmClientContext) : GmmMemory(gmmClientContext){};

    bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                     GMM_ESCAPE_HANDLE hDevice,
                                     GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                     GMM_GFX_SIZE_T SvmSize,
                                     BOOLEAN BDWL3Coherency) override {
        return true;
    }

    uintptr_t getInternalGpuVaRangeLimit() override {
        return NEO::windowsMinAddress;
    }

    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override {
        partition = deviceInfo->pGfxPartition;
        deviceCallbacks = *deviceInfo->pDeviceCb;
        return setDeviceInfoValue;
    }

    GMM_GFX_PARTITIONING *partition = nullptr;
    bool setDeviceInfoValue = true;
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
};

class GmockGmmMemoryBase : public GmmMemory {
  public:
    ~GmockGmmMemoryBase() = default;

    GmockGmmMemoryBase(GmmClientContext *gmmClientContext) : GmmMemory(gmmClientContext) {
        ON_CALL(*this, getInternalGpuVaRangeLimit())
            .WillByDefault(::testing::Return(NEO::windowsMinAddress));

        ON_CALL(*this, setDeviceInfo(::testing::_))
            .WillByDefault(::testing::Return(true));
    }

    MOCK_METHOD0(getInternalGpuVaRangeLimit, uintptr_t());

    MOCK_METHOD1(setDeviceInfo, bool(GMM_DEVICE_INFO *));

    MOCK_METHOD5(configureDeviceAddressSpace,
                 bool(GMM_ESCAPE_HANDLE hAdapter,
                      GMM_ESCAPE_HANDLE hDevice,
                      GMM_ESCAPE_FUNC_TYPE pfnEscape,
                      GMM_GFX_SIZE_T SvmSize,
                      BOOLEAN BDWL3Coherency));
};
} // namespace NEO
