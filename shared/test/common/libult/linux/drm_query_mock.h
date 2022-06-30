/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

using namespace NEO;

class DrmQueryMock : public DrmMock {
  public:
    using Drm::rootDeviceEnvironment;

    DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmQueryMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment, const HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        context.hwInfo = rootDeviceEnvironment.getHardwareInfo();
        callBaseIsVmBindAvailable = true;

        this->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*this);

        EXPECT_TRUE(queryMemoryInfo());
        EXPECT_EQ(2u + virtualMemoryIds.size(), ioctlCallsCount);
        ioctlCallsCount = 0;
    }

    DrmMockPrelimContext context{
        nullptr,
        rootDeviceEnvironment,
        getCacheInfo(),
        failRetTopology,
        supportedCopyEnginesMask,
        contextDebugSupported,
    };

    static constexpr uint32_t maxEngineCount{9};
    I915_DEFINE_CONTEXT_ENGINES_LOAD_BALANCE(receivedContextEnginesLoadBalance, maxEngineCount){};
    I915_DEFINE_CONTEXT_PARAM_ENGINES(receivedContextParamEngines, 1 + maxEngineCount){};

    BcsInfoMask supportedCopyEnginesMask = 1;
    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    int storedRetValForSetParamEngines{0};

    int handleRemainingRequests(DrmIoctl request, void *arg) override;
    virtual bool handleQueryItem(void *queryItem);
};
