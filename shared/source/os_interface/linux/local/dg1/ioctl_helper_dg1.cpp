/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/dg1/drm/i915_drm.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_DG1;

extern bool isQueryDrmTip(const std::vector<uint8_t> &queryInfo);
extern std::vector<uint8_t> translateToDrmTip(const uint8_t *dataQuery);

template <>
IoctlHelper *IoctlHelperImpl<gfxProduct>::clone() {
    return new IoctlHelperImpl<gfxProduct>{};
}

template <>
uint32_t IoctlHelperImpl<gfxProduct>::createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) {
    auto ret = IoctlHelperUpstream::createGemExt(drm, memClassInstances, allocSize, handle);
    if (ret == 0) {
        return ret;
    }
    //fallback to PROD_DG1 kernel
    handle = 0u;
    uint32_t regionsSize = static_cast<uint32_t>(memClassInstances.size());
    std::vector<drm_i915_gem_memory_class_instance> regions(regionsSize);
    for (auto i = 0u; i < regionsSize; i++) {
        regions[i].memory_class = memClassInstances[i].memoryClass;
        regions[i].memory_instance = memClassInstances[i].memoryInstance;
    }

    drm_i915_gem_object_param regionParam{};
    regionParam.size = regionsSize;
    regionParam.data = reinterpret_cast<uintptr_t>(regions.data());
    regionParam.param = I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS;

    drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);

    ret = IoctlHelper::ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    handle = createExt.handle;
    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_SETPARAM has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    return ret;
}

template <>
std::vector<MemoryRegion> IoctlHelperImpl<gfxProduct>::translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) {
    if (!isQueryDrmTip(regionInfo)) {
        auto translated = translateToDrmTip(regionInfo.data());
        return IoctlHelperUpstream::translateToMemoryRegions(translated);
    }
    return IoctlHelperUpstream::translateToMemoryRegions(regionInfo);
}

template class IoctlHelperImpl<gfxProduct>;
} // namespace NEO
