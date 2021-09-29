/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "drm_tip.h"

namespace NEO {

uint32_t MemoryInfoImpl::createGemExtMemoryRegions(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    DRM_TIP::drm_i915_gem_create_ext_memory_regions extRegions{};
    extRegions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;
    extRegions.num_regions = dataSize;
    extRegions.regions = reinterpret_cast<uintptr_t>(data);

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&extRegions);

    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    handle = createExt.handle;
    return ret;
}

} // namespace NEO
