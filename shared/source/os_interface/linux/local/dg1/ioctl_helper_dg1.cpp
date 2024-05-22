/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {
namespace Dg1I915 {
#include "third_party/uapi/dg1/i915_drm.h"
}

using namespace Dg1I915;

constexpr static auto gfxProduct = IGFX_DG1;

extern bool isQueryDrmTip(const std::vector<uint64_t> &queryInfo);

template <>
int IoctlHelperImpl<gfxProduct>::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) {
    auto ret = IoctlHelperUpstream::createGemExt(memClassInstances, allocSize, handle, patIndex, vmId, pairHandle, isChunked, numOfChunks, memPolicyMode, memPolicyNodemask, isCoherent);
    if (ret == 0) {
        return ret;
    }
    // fallback to PROD_DG1 kernel
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

    ret = IoctlHelper::ioctl(DrmIoctl::dg1GemCreateExt, &createExt);

    handle = createExt.handle;
    printDebugString(debugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_SETPARAM has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    return ret;
}

std::vector<MemoryRegion> translateDg1RegionInfoToMemoryRegions(const std::vector<uint64_t> &regionInfo) {
    auto *data = reinterpret_cast<const drm_i915_query_memory_regions *>(regionInfo.data());
    auto memRegions = std::vector<MemoryRegion>(data->num_regions);
    for (uint32_t i = 0; i < data->num_regions; i++) {
        memRegions[i].probedSize = data->regions[i].probed_size;
        memRegions[i].unallocatedSize = data->regions[i].unallocated_size;
        memRegions[i].region.memoryClass = data->regions[i].region.memory_class;
        memRegions[i].region.memoryInstance = data->regions[i].region.memory_instance;
    }
    return memRegions;
}

template <>
std::vector<MemoryRegion> IoctlHelperImpl<gfxProduct>::translateToMemoryRegions(const std::vector<uint64_t> &regionInfo) {
    if (!isQueryDrmTip(regionInfo)) {
        return translateDg1RegionInfoToMemoryRegions(regionInfo);
    }
    return IoctlHelperUpstream::translateToMemoryRegions(regionInfo);
}

template <>
unsigned int IoctlHelperImpl<gfxProduct>::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::dg1GemCreateExt:
        return DRM_IOCTL_I915_GEM_CREATE_EXT;
    default:
        return IoctlHelperUpstream::getIoctlRequestValue(ioctlRequest);
    }
}

template <>
std::string IoctlHelperImpl<gfxProduct>::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::dg1GemCreateExt:
        return "DRM_IOCTL_I915_GEM_CREATE_EXT";
    default:
        return IoctlHelperUpstream::getIoctlString(ioctlRequest);
    }
}

template class IoctlHelperImpl<gfxProduct>;
} // namespace NEO
