/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/drm/i915_drm.h"

namespace NEO {

uint32_t IoctlHelperUpstream::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    drm_i915_gem_create_ext_memory_regions memRegions{};
    memRegions.num_regions = dataSize;
    memRegions.regions = reinterpret_cast<uintptr_t>(data);
    memRegions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&memRegions);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu",
                     allocSize);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < dataSize; i++) {
            auto region = reinterpret_cast<drm_i915_gem_memory_class_instance *>(data)[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

std::unique_ptr<uint8_t[]> IoctlHelperUpstream::translateIfRequired(uint8_t *dataQuery, int32_t length) {
    return std::unique_ptr<uint8_t[]>(dataQuery);
}

CacheRegion IoctlHelperUpstream::closAlloc(Drm *drm) {
    return CacheRegion::None;
}

uint16_t IoctlHelperUpstream::closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) {
    return 0;
}

CacheRegion IoctlHelperUpstream::closFree(Drm *drm, CacheRegion closIndex) {
    return CacheRegion::None;
}

int IoctlHelperUpstream::waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                                       uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) {
    return 0;
}

uint32_t IoctlHelperUpstream::getHwConfigIoctlVal() {
    return DRM_I915_QUERY_HWCONFIG_TABLE;
}

uint32_t IoctlHelperUpstream::getAtomicAdvise(bool isNonAtomic) {
    return 0;
}

uint32_t IoctlHelperUpstream::getPreferredLocationAdvise() {
    return 0;
}

bool IoctlHelperUpstream::setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) {
    return true;
}

} // namespace NEO
