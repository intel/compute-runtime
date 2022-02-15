/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_query_mock.h"

#include "shared/source/helpers/ptr_math.h"

#include "gtest/gtest.h"

int DrmQueryMock::handleRemainingRequests(unsigned long request, void *arg) {
    if (request == DRM_IOCTL_I915_QUERY && arg) {
        if (i915QuerySuccessCount == 0) {
            return EINVAL;
        }
        i915QuerySuccessCount--;

        auto query = static_cast<drm_i915_query *>(arg);
        if (query->items_ptr == 0) {
            return EINVAL;
        }

        for (auto i = 0u; i < query->num_items; ++i) {
            const auto queryItem = reinterpret_cast<drm_i915_query_item *>(query->items_ptr) + i;
            if (!this->handleQueryItem(queryItem)) {
                return EINVAL;
            }
        }

        return 0;
    } else if (request == DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM && receivedContextParamRequest.param == I915_CONTEXT_PARAM_ENGINES) {
        EXPECT_LE(receivedContextParamRequest.size, sizeof(receivedContextParamEngines));
        memcpy(&receivedContextParamEngines, reinterpret_cast<const void *>(receivedContextParamRequest.value), receivedContextParamRequest.size);
        auto srcBalancer = reinterpret_cast<const i915_context_engines_load_balance *>(receivedContextParamEngines.extensions);
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
