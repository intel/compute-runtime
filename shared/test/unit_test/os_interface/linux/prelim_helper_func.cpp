/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>
#include <sys/ioctl.h>

using namespace NEO;

int handlePrelimRequests(DrmIoctl request, void *arg, int ioctlRetVal, int queryDistanceIoctlRetVal) {
    if (request == DrmIoctl::gemCreateExt) {
        auto createExtParams = static_cast<prelim_drm_i915_gem_create_ext *>(arg);
        if (createExtParams->size == 0) {
            return EINVAL;
        }
        createExtParams->handle = 1u;
        auto extensions = reinterpret_cast<prelim_drm_i915_gem_create_ext_setparam *>(createExtParams->extensions);
        if (extensions == nullptr) {
            return EINVAL;
        }
        auto setparamRegion = *extensions;
        if (setparamRegion.base.name != PRELIM_I915_GEM_CREATE_EXT_SETPARAM) {
            return EINVAL;
        }
        if ((setparamRegion.param.size == 0) ||
            (setparamRegion.param.param != (PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS))) {
            return EINVAL;
        }
        if (setparamRegion.base.next_extension != 0) {
            auto vmPrivate = reinterpret_cast<prelim_drm_i915_gem_create_ext_vm_private *>(setparamRegion.base.next_extension);
            if (vmPrivate->base.name != PRELIM_I915_GEM_CREATE_EXT_VM_PRIVATE) {
                return EINVAL;
            }
        }
        auto data = reinterpret_cast<MemoryClassInstance *>(setparamRegion.param.data);
        if (data == nullptr) {
            return EINVAL;
        }

        if ((data->memoryClass != prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM) && (data->memoryClass != prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE)) {
            return EINVAL;
        }
    } else if (request == DrmIoctl::gemClosReserve) {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        closReserveArg->clos_index = 1u;
    } else if (request == DrmIoctl::query) {
        auto query = static_cast<Query *>(arg);
        if (query->itemsPtr == 0) {
            return EINVAL;
        }
        for (auto i = 0u; i < query->numItems; i++) {
            auto queryItemPtr = reinterpret_cast<QueryItem *>(query->itemsPtr) + i;
            if (queryItemPtr->queryId == PRELIM_DRM_I915_QUERY_DISTANCE_INFO) {
                if (queryDistanceIoctlRetVal != 0) {
                    return queryDistanceIoctlRetVal;
                }
                auto distance = reinterpret_cast<prelim_drm_i915_query_distance_info *>(queryItemPtr->dataPtr);
                distance->distance = (distance->engine.engine_instance == distance->region.memory_instance) ? 0 : 100;
            } else if (queryItemPtr->queryId == DRM_I915_QUERY_ENGINE_INFO) {
                auto numberOfTiles = 2u;
                uint32_t numberOfEngines = numberOfTiles * 6u;
                int engineInfoSize = sizeof(drm_i915_query_engine_info) + numberOfEngines * sizeof(drm_i915_engine_info);
                if (queryItemPtr->length == 0) {
                    queryItemPtr->length = engineInfoSize;
                } else {
                    EXPECT_EQ(engineInfoSize, queryItemPtr->length);
                    auto queryEngineInfo = reinterpret_cast<drm_i915_query_engine_info *>(queryItemPtr->dataPtr);
                    EXPECT_EQ(0u, queryEngineInfo->num_engines);
                    queryEngineInfo->num_engines = numberOfEngines;
                    auto p = queryEngineInfo->engines;
                    for (uint16_t tile = 0u; tile < numberOfTiles; tile++) {
                        p++->engine = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, tile};
                        p++->engine = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, tile};
                        p++->engine = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, tile};
                        p++->engine = {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE, tile};
                        p++->engine = {prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE, tile};
                        p++->engine = {UINT16_MAX, tile};
                    }
                }
                break;
            }
        }
    } else if (request == DrmIoctl::gemVmPrefetch) {
        auto vmPrefetchParams = static_cast<prelim_drm_i915_gem_vm_prefetch *>(arg);
        // Valid vm_id must be nonzero
        EXPECT_NE(0u, vmPrefetchParams->vm_id);
    }
    return ioctlRetVal;
}

std::vector<uint64_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions) {
    auto inputSize = static_cast<uint32_t>(inputRegions.size());
    int length = sizeof(drm_i915_query_memory_regions) + inputSize * sizeof(drm_i915_memory_region_info);
    auto data = std::vector<uint64_t>(Math::divideAndRoundUp(length, sizeof(uint64_t)));
    auto memoryRegions = reinterpret_cast<drm_i915_query_memory_regions *>(data.data());
    memoryRegions->num_regions = inputSize;

    for (uint32_t i = 0; i < inputSize; i++) {
        memoryRegions->regions[i].region.memory_class = inputRegions[i].region.memoryClass;
        memoryRegions->regions[i].region.memory_instance = inputRegions[i].region.memoryInstance;
        memoryRegions->regions[i].probed_size = inputRegions[i].probedSize;
        memoryRegions->regions[i].unallocated_size = inputRegions[i].unallocatedSize;
        memoryRegions->regions[i].rsvd1[0] = inputRegions[i].cpuVisibleSize;
    }
    return data;
}

std::vector<uint64_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines) {
    auto inputSize = static_cast<uint32_t>(inputEngines.size());
    int length = sizeof(drm_i915_query_engine_info) + inputSize * sizeof(drm_i915_engine_info);
    auto data = std::vector<uint64_t>(Math::divideAndRoundUp(length, sizeof(uint64_t)));
    auto memoryRegions = reinterpret_cast<drm_i915_query_engine_info *>(data.data());
    memoryRegions->num_engines = inputSize;

    for (uint32_t i = 0; i < inputSize; i++) {
        memoryRegions->engines[i].engine.engine_class = inputEngines[i].engine.engineClass;
        memoryRegions->engines[i].engine.engine_instance = inputEngines[i].engine.engineInstance;
        memoryRegions->engines[i].capabilities |= inputEngines[i].capabilities.copyClassSaturateLink ? PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK : 0;
        memoryRegions->engines[i].capabilities |= inputEngines[i].capabilities.copyClassSaturatePCIE ? PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE : 0;
    }
    return data;
}
