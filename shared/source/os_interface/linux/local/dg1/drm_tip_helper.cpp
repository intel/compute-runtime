/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/i915.h"

#include <memory>
#include <vector>

namespace NEO {

bool isQueryDrmTip(const std::vector<uint64_t> &queryInfo) {
    auto dataOnDrmTip = reinterpret_cast<const drm_i915_query_memory_regions *>(queryInfo.data());
    auto lengthOnDrmTip = static_cast<uint32_t>(sizeof(drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(drm_i915_memory_region_info));
    return static_cast<uint32_t>(queryInfo.size() * sizeof(uint64_t)) == lengthOnDrmTip;
}

} // namespace NEO
