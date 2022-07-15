/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/i915_upstream.h"

#include <memory>
#include <vector>

namespace NEO {

bool isQueryDrmTip(const std::vector<uint8_t> &queryInfo) {
    auto dataOnDrmTip = reinterpret_cast<const drm_i915_query_memory_regions *>(queryInfo.data());
    auto lengthOnDrmTip = static_cast<uint32_t>(sizeof(drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(drm_i915_memory_region_info));
    return static_cast<uint32_t>(queryInfo.size()) == lengthOnDrmTip;
}

} // namespace NEO
