/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

class DrmMockDg1 : public DrmMock {
  public:
    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    uint32_t queryMemoryRegionInfoSuccessCount = std::numeric_limits<uint32_t>::max();

    //DRM_IOCTL_I915_GEM_CREATE_EXT
    drm_i915_gem_create_ext createExt{};
    drm_i915_gem_create_ext_setparam setparamRegion{};
    drm_i915_gem_memory_class_instance memRegions{};

    //DRM_IOCTL_I915_GEM_MMAP_OFFSET
    __u64 offset = 0;

    virtual int handleRemainingRequests(unsigned long request, void *arg) {
        if ((request == DRM_IOCTL_I915_QUERY) && (arg != nullptr)) {
            if (i915QuerySuccessCount == 0) {
                return EINVAL;
            }
            i915QuerySuccessCount--;
            auto query = static_cast<drm_i915_query *>(arg);
            if (query->items_ptr == 0) {
                return EINVAL;
            }
            for (auto i = 0u; i < query->num_items; i++) {
                handleQueryItem(reinterpret_cast<drm_i915_query_item *>(query->items_ptr) + i);
            }
            return 0;
        } else if (request == DRM_IOCTL_I915_GEM_CREATE_EXT) {
            auto createExtParams = static_cast<drm_i915_gem_create_ext *>(arg);
            if (createExtParams->size == 0) {
                return EINVAL;
            }
            this->createExt.size = createExtParams->size;
            this->createExt.handle = createExtParams->handle = 1u;
            auto extensions = reinterpret_cast<drm_i915_gem_create_ext_setparam *>(createExtParams->extensions);
            if (extensions == nullptr) {
                return EINVAL;
            }
            this->setparamRegion = *extensions;
            if (this->setparamRegion.base.name != I915_GEM_CREATE_EXT_SETPARAM) {
                return EINVAL;
            }
            if ((this->setparamRegion.param.size == 0) ||
                (this->setparamRegion.param.param != (I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS))) {
                return EINVAL;
            }
            auto data = reinterpret_cast<drm_i915_gem_memory_class_instance *>(this->setparamRegion.param.data);
            if (data == nullptr) {
                return EINVAL;
            }
            this->memRegions = *data;
            if ((this->memRegions.memory_class != I915_MEMORY_CLASS_SYSTEM) && (this->memRegions.memory_class != I915_MEMORY_CLASS_DEVICE)) {
                return EINVAL;
            }
            return 0;

        } else if (request == DRM_IOCTL_I915_GEM_MMAP_OFFSET) {
            auto mmap_arg = static_cast<drm_i915_gem_mmap_offset *>(arg);
            mmap_arg->offset = offset;
            return 0;
        }
        return -1;
    }

    void handleQueryItem(drm_i915_query_item *queryItem) {
        switch (queryItem->query_id) {
        case DRM_I915_QUERY_MEMORY_REGIONS:
            if (queryMemoryRegionInfoSuccessCount == 0) {
                queryItem->length = -EINVAL;
            } else {
                queryMemoryRegionInfoSuccessCount--;
                auto numberOfLocalMemories = 1u;
                auto numberOfRegions = 1u + numberOfLocalMemories;
                int regionInfoSize = sizeof(drm_i915_query_memory_regions) + numberOfRegions * sizeof(drm_i915_memory_region_info);
                if (queryItem->length == 0) {
                    queryItem->length = regionInfoSize;
                } else {
                    EXPECT_EQ(regionInfoSize, queryItem->length);
                    auto queryMemoryRegionInfo = reinterpret_cast<drm_i915_query_memory_regions *>(queryItem->data_ptr);
                    EXPECT_EQ(0u, queryMemoryRegionInfo->num_regions);
                    queryMemoryRegionInfo->num_regions = numberOfRegions;
                    queryMemoryRegionInfo->regions[0].region.memory_class = I915_MEMORY_CLASS_SYSTEM;
                    queryMemoryRegionInfo->regions[0].region.memory_instance = 1;
                    queryMemoryRegionInfo->regions[0].probed_size = 2 * MemoryConstants::gigaByte;
                    queryMemoryRegionInfo->regions[1].region.memory_class = I915_MEMORY_CLASS_DEVICE;
                    queryMemoryRegionInfo->regions[1].region.memory_instance = 1;
                    queryMemoryRegionInfo->regions[1].probed_size = 2 * MemoryConstants::gigaByte;
                }
            }
            break;
        }
    }
};
