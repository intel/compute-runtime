/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

using namespace NEO;
template <uint32_t numEngines = 10> // 1 + max engines
struct ContextParamEnginesI915 {
    uint64_t extensions;
    EngineClassInstance engines[numEngines];
};

class DrmQueryMock : public DrmMock {
  public:
    using Drm::ioctlHelper;
    using Drm::rootDeviceEnvironment;

    DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment);

    DrmMockPrelimContext context{
        nullptr,
        rootDeviceEnvironment,
        getCacheInfo(),
        failRetTopology,
        supportedCopyEnginesMask,
        contextDebugSupported,
    };

    int getEuDebugSysFsEnable() override {
        return prelimEuDebugValue;
    }
    int prelimEuDebugValue = 0;

    static constexpr uint32_t maxEngineCount{9};
    ContextEnginesLoadBalance<maxEngineCount> receivedContextEnginesLoadBalance{};
    ContextParamEnginesI915<1 + maxEngineCount> receivedContextParamEngines{};

    BcsInfoMask supportedCopyEnginesMask = 1;
    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    int storedRetValForSetParamEngines{0};

    int handleRemainingRequests(DrmIoctl request, void *arg) override;
    virtual bool handleQueryItem(void *queryItem);
};
