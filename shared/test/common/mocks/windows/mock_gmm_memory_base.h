/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "gmm_memory.h"
#include "gtest/gtest.h"

namespace NEO {

class MockGmmMemoryBase : public GmmMemory {
  public:
    ~MockGmmMemoryBase() = default;

    MockGmmMemoryBase(GmmClientContext *gmmClientContext) : GmmMemory(gmmClientContext){};

    bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                     GMM_ESCAPE_HANDLE hDevice,
                                     GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                     GMM_GFX_SIZE_T svmSize,
                                     BOOLEAN bdwL3Coherency) override {
        configureDeviceAddressSpaceCalled++;
        configureDeviceAddressSpaceParamsPassed.push_back({hAdapter,
                                                           hDevice,
                                                           pfnEscape,
                                                           svmSize,
                                                           bdwL3Coherency});
        return configureDeviceAddressSpaceResult;
    }

    struct ConfigureDeviceAddressSpaceParams {
        GMM_ESCAPE_HANDLE hAdapter{};
        GMM_ESCAPE_HANDLE hDevice{};
        GMM_ESCAPE_FUNC_TYPE pfnEscape{};
        GMM_GFX_SIZE_T svmSize{};
        BOOLEAN bdwL3Coherency{};
    };

    uint32_t configureDeviceAddressSpaceCalled = 0u;
    bool configureDeviceAddressSpaceResult = true;
    StackVec<ConfigureDeviceAddressSpaceParams, 1> configureDeviceAddressSpaceParamsPassed{};

    void overrideInternalGpuVaRangeLimit(uintptr_t value) {
        this->getInternalGpuVaRangeLimitResult = value;
    }

    ADDMETHOD_NOBASE(getInternalGpuVaRangeLimit, uintptr_t, NEO::windowsMinAddress, ());

    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override {
        setDeviceInfoCalled++;
        partition = deviceInfo->pGfxPartition;
        deviceCallbacks = *deviceInfo->pDeviceCb;
        return setDeviceInfoResult;
    }

    uint32_t setDeviceInfoCalled = 0u;
    bool setDeviceInfoResult = true;
    GMM_GFX_PARTITIONING *partition = nullptr;
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
};
} // namespace NEO
