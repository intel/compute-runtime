/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

#include <memory>
#include <sys/ioctl.h>

using namespace NEO;

int handlePrelimRequests(unsigned long request, void *arg, int ioctlRetVal, int queryDistanceIoctlRetVal) {
    if (request == PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT) {
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
        auto data = reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(setparamRegion.param.data);
        if (data == nullptr) {
            return EINVAL;
        }

        if ((data->memory_class != PRELIM_I915_MEMORY_CLASS_SYSTEM) && (data->memory_class != PRELIM_I915_MEMORY_CLASS_DEVICE)) {
            return EINVAL;
        }
    } else if (request == PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE) {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        closReserveArg->clos_index = 1u;
    } else if (request == DRM_IOCTL_I915_QUERY) {
        auto query = static_cast<drm_i915_query *>(arg);
        if (query->items_ptr == 0) {
            return EINVAL;
        }
        for (auto i = 0u; i < query->num_items; i++) {
            auto queryItemPtr = reinterpret_cast<drm_i915_query_item *>(query->items_ptr) + i;
            if (queryItemPtr->query_id == PRELIM_DRM_I915_QUERY_DISTANCE_INFO) {
                if (queryDistanceIoctlRetVal != 0) {
                    return queryDistanceIoctlRetVal;
                }
                auto distance = reinterpret_cast<prelim_drm_i915_query_distance_info *>(queryItemPtr->data_ptr);
                distance->distance = (distance->engine.engine_instance == distance->region.memory_instance) ? 0 : 100;
            } else if (queryItemPtr->query_id == PRELIM_DRM_I915_QUERY_ENGINE_INFO) {
                auto numberOfTiles = 2u;
                uint32_t numberOfEngines = numberOfTiles * 6u;
                int engineInfoSize = sizeof(prelim_drm_i915_query_engine_info) + numberOfEngines * sizeof(prelim_drm_i915_engine_info);
                if (queryItemPtr->length == 0) {
                    queryItemPtr->length = engineInfoSize;
                } else {
                    EXPECT_EQ(engineInfoSize, queryItemPtr->length);
                    auto queryEngineInfo = reinterpret_cast<prelim_drm_i915_query_engine_info *>(queryItemPtr->data_ptr);
                    EXPECT_EQ(0u, queryEngineInfo->num_engines);
                    queryEngineInfo->num_engines = numberOfEngines;
                    auto p = queryEngineInfo->engines;
                    for (uint16_t tile = 0u; tile < numberOfTiles; tile++) {
                        p++->engine = {I915_ENGINE_CLASS_RENDER, tile};
                        p++->engine = {I915_ENGINE_CLASS_COPY, tile};
                        p++->engine = {I915_ENGINE_CLASS_VIDEO, tile};
                        p++->engine = {I915_ENGINE_CLASS_VIDEO_ENHANCE, tile};
                        p++->engine = {PRELIM_I915_ENGINE_CLASS_COMPUTE, tile};
                        p++->engine = {UINT16_MAX, tile};
                    }
                }
                break;
            }
        }
    }
    return ioctlRetVal;
}

std::vector<uint8_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions) {
    auto inputSize = static_cast<uint32_t>(inputRegions.size());
    int length = sizeof(prelim_drm_i915_query_memory_regions) + inputSize * sizeof(prelim_drm_i915_memory_region_info);
    auto data = std::vector<uint8_t>(length);
    auto memoryRegions = reinterpret_cast<prelim_drm_i915_query_memory_regions *>(data.data());
    memoryRegions->num_regions = inputSize;

    for (uint32_t i = 0; i < inputSize; i++) {
        memoryRegions->regions[i].region.memory_class = inputRegions[i].region.memoryClass;
        memoryRegions->regions[i].region.memory_instance = inputRegions[i].region.memoryInstance;
        memoryRegions->regions[i].probed_size = inputRegions[i].probedSize;
        memoryRegions->regions[i].unallocated_size = inputRegions[i].unallocatedSize;
    }
    return data;
}

std::vector<uint8_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines) {
    auto inputSize = static_cast<uint32_t>(inputEngines.size());
    int length = sizeof(prelim_drm_i915_query_engine_info) + inputSize * sizeof(prelim_drm_i915_engine_info);
    auto data = std::vector<uint8_t>(length);
    auto memoryRegions = reinterpret_cast<prelim_drm_i915_query_engine_info *>(data.data());
    memoryRegions->num_engines = inputSize;

    for (uint32_t i = 0; i < inputSize; i++) {
        memoryRegions->engines[i].engine.engine_class = inputEngines[i].engine.engineClass;
        memoryRegions->engines[i].engine.engine_instance = inputEngines[i].engine.engineInstance;
        memoryRegions->engines[i].capabilities = inputEngines[i].capabilities;
    }
    return data;
}
