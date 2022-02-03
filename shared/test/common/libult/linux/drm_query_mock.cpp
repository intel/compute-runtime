/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_query_mock.h"

int DrmQueryMock::handleRemainingRequests(unsigned long request, void *arg) {
    if (request == DRM_IOCTL_I915_QUERY && arg) {
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
    }

    return context.handlePrelimRequest(request, arg);
}

bool DrmQueryMock::handleQueryItem(void *arg) {
    return context.handlePrelimQueryItem(arg);
}
