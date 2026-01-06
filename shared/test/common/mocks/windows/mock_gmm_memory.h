/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/windows/gmm_memory.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

class MockGmmMemory : public GmmMemory {
  public:
    ~MockGmmMemory() override = default;

    MockGmmMemory(GmmClientContext *gmmClientContext) : GmmMemory(gmmClientContext){};

    bool configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                     GMM_ESCAPE_HANDLE hDevice,
                                     GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                     GMM_GFX_SIZE_T svmSize,
                                     BOOLEAN bdwL3Coherency) override;

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

    void overrideInternalGpuVaRangeLimit(uintptr_t value);

    ADDMETHOD_NOBASE(getInternalGpuVaRangeLimit, uintptr_t, NEO::windowsMinAddress, ());

    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override;

    uint32_t setDeviceInfoCalled = 0u;
    bool setDeviceInfoResult = true;
    GMM_GFX_PARTITIONING *partition = nullptr;
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
};
} // namespace NEO
