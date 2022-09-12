/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_query_mock.h"

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/i915.h"

#include "gtest/gtest.h"

DrmQueryMock::DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment, const HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
    rootDeviceEnvironment.setHwInfo(inputHwInfo);
    context.hwInfo = rootDeviceEnvironment.getHardwareInfo();
    callBaseIsVmBindAvailable = true;

    this->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*this);

    EXPECT_TRUE(queryMemoryInfo());
    EXPECT_EQ(2u + getBaseIoctlCalls(), ioctlCallsCount);
    ioctlCallsCount = 0;
}

int DrmQueryMock::handleRemainingRequests(DrmIoctl request, void *arg) {
    if (request == DrmIoctl::Query && arg) {
        if (i915QuerySuccessCount == 0) {
            return EINVAL;
        }
        i915QuerySuccessCount--;

        auto query = static_cast<Query *>(arg);
        if (query->itemsPtr == 0) {
            return EINVAL;
        }

        for (auto i = 0u; i < query->numItems; ++i) {
            const auto queryItem = reinterpret_cast<QueryItem *>(query->itemsPtr) + i;
            if (!this->handleQueryItem(queryItem)) {
                return EINVAL;
            }
        }

        return 0;
    } else if (request == DrmIoctl::GemContextSetparam && receivedContextParamRequest.param == I915_CONTEXT_PARAM_ENGINES) {
        EXPECT_LE(receivedContextParamRequest.size, sizeof(receivedContextParamEngines));
        memcpy(&receivedContextParamEngines, reinterpret_cast<const void *>(receivedContextParamRequest.value), receivedContextParamRequest.size);
        auto srcBalancer = reinterpret_cast<const I915::i915_context_engines_load_balance *>(receivedContextParamEngines.extensions);
        if (srcBalancer) {
            EXPECT_EQ(static_cast<__u32>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE), srcBalancer->base.name);
            auto balancerSize = ptrDiff(srcBalancer->engines + srcBalancer->num_siblings, srcBalancer);
            EXPECT_LE(balancerSize, sizeof(receivedContextEnginesLoadBalance));
            memcpy(&receivedContextEnginesLoadBalance, srcBalancer, balancerSize);
        }
        return storedRetValForSetParamEngines;
    }

    return context.handlePrelimRequest(request, arg);
}

bool DrmQueryMock::handleQueryItem(void *arg) {
    return context.handlePrelimQueryItem(arg);
}
