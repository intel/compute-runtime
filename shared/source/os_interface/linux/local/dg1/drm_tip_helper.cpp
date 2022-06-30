/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/drm/i915_drm.h"

#include <memory>

namespace NEO {

bool isQueryDrmTip(const std::vector<uint8_t> &queryInfo) {
    auto dataOnDrmTip = reinterpret_cast<const drm_i915_query_memory_regions *>(queryInfo.data());
    auto lengthOnDrmTip = static_cast<uint32_t>(sizeof(drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(drm_i915_memory_region_info));
    return static_cast<uint32_t>(queryInfo.size()) == lengthOnDrmTip;
}

namespace PROD_DG1 {
#undef DRM_IOCTL_I915_GEM_CREATE_EXT
#undef __I915_EXEC_UNKNOWN_FLAGS
#include "third_party/uapi/dg1/drm/i915_drm.h"
} // namespace PROD_DG1

std::vector<uint8_t> translateToDrmTip(const uint8_t *dataQuery) {
    auto dataOnProdDrm = reinterpret_cast<const PROD_DG1::drm_i915_query_memory_regions *>(dataQuery);
    auto lengthTranslated = static_cast<int32_t>(sizeof(drm_i915_query_memory_regions) + dataOnProdDrm->num_regions * sizeof(drm_i915_memory_region_info));
    auto dataQueryTranslated = std::vector<uint8_t>(lengthTranslated, 0u);
    auto dataTranslated = reinterpret_cast<drm_i915_query_memory_regions *>(dataQueryTranslated.data());
    dataTranslated->num_regions = dataOnProdDrm->num_regions;
    for (uint32_t i = 0; i < dataTranslated->num_regions; i++) {
        dataTranslated->regions[i].region.memory_class = dataOnProdDrm->regions[i].region.memory_class;
        dataTranslated->regions[i].region.memory_instance = dataOnProdDrm->regions[i].region.memory_instance;
        dataTranslated->regions[i].probed_size = dataOnProdDrm->regions[i].probed_size;
        dataTranslated->regions[i].unallocated_size = dataOnProdDrm->regions[i].unallocated_size;
    }
    return dataQueryTranslated;
}

} // namespace NEO
