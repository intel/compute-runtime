/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "third_party/uapi/drm_tip/drm/i915_drm.h"

#include <memory>

namespace NEO {

uint32_t createGemExtMemoryRegions(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    drm_i915_gem_create_ext_memory_regions extRegions{};
    extRegions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;
    extRegions.num_regions = dataSize;
    extRegions.regions = reinterpret_cast<uintptr_t>(data);

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&extRegions);

    auto ret = LocalMemoryHelper::ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    handle = createExt.handle;
    return ret;
}

bool isQueryDrmTip(uint8_t *dataQuery, int32_t length) {
    auto dataOnDrmTip = reinterpret_cast<drm_i915_query_memory_regions *>(dataQuery);
    auto lengthOnDrmTip = static_cast<int32_t>(sizeof(drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(drm_i915_memory_region_info));
    return length == lengthOnDrmTip;
}

namespace PROD_DG1 {
#undef DRM_IOCTL_I915_GEM_CREATE_EXT
#undef __I915_EXEC_UNKNOWN_FLAGS
#include "third_party/uapi/dg1/drm/i915_drm.h"
} // namespace PROD_DG1

std::unique_ptr<uint8_t[]> translateToDrmTip(uint8_t *dataQuery, int32_t &length) {
    auto dataOnProdDrm = reinterpret_cast<PROD_DG1::drm_i915_query_memory_regions *>(dataQuery);
    auto lengthTranslated = static_cast<int32_t>(sizeof(drm_i915_query_memory_regions) + dataOnProdDrm->num_regions * sizeof(drm_i915_memory_region_info));
    auto dataQueryTranslated = std::make_unique<uint8_t[]>(lengthTranslated);
    auto dataTranslated = reinterpret_cast<drm_i915_query_memory_regions *>(dataQueryTranslated.get());
    dataTranslated->num_regions = dataOnProdDrm->num_regions;
    for (uint32_t i = 0; i < dataTranslated->num_regions; i++) {
        dataTranslated->regions[i].region.memory_class = dataOnProdDrm->regions[i].region.memory_class;
        dataTranslated->regions[i].region.memory_instance = dataOnProdDrm->regions[i].region.memory_instance;
        dataTranslated->regions[i].probed_size = dataOnProdDrm->regions[i].probed_size;
        dataTranslated->regions[i].unallocated_size = dataOnProdDrm->regions[i].unallocated_size;
    }
    length = lengthTranslated;
    return dataQueryTranslated;
}

} // namespace NEO
