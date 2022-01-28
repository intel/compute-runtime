/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_prelim_helper.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

uint32_t getQueryComputeSlicesIoctl() {
    return PRELIM_DRM_I915_QUERY_COMPUTE_SLICES;
}
