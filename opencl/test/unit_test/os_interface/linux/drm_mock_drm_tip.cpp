/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_exp.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_exp.h"

// clang-format off
#include "shared/source/os_interface/linux/drm_tip.h"
// clang-format on

void DrmMockExp::handleQueryItemOnDrmTip(drm_i915_query_item *queryItem) {
    switch (queryItem->query_id) {
    case DRM_I915_QUERY_MEMORY_REGIONS:
        if (queryMemoryRegionInfoSuccessCount == 0) {
            queryItem->length = -EINVAL;
        } else {
            queryMemoryRegionInfoSuccessCount--;
            auto numberOfLocalMemories = 1u;
            auto numberOfRegions = 1u + numberOfLocalMemories;
            int regionInfoSize = sizeof(DRM_TIP::drm_i915_query_memory_regions) + numberOfRegions * sizeof(DRM_TIP::drm_i915_memory_region_info);
            if (queryItem->length == 0) {
                queryItem->length = regionInfoSize;
            } else {
                EXPECT_EQ(regionInfoSize, queryItem->length);
                auto queryMemoryRegionInfo = reinterpret_cast<DRM_TIP::drm_i915_query_memory_regions *>(queryItem->data_ptr);
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

int DrmMockCustomExp::ioctlGemCreateExt(unsigned long request, void *arg) {
    if (request == DRM_IOCTL_I915_GEM_CREATE_EXT) {
        auto createExtParams = reinterpret_cast<DRM_TIP::drm_i915_gem_create_ext *>(arg);
        createExtSize = createExtParams->size;
        createExtHandle = createExtParams->handle;
        createExtExtensions = createExtParams->extensions;
        ioctlExp_cnt.gemCreateExt++;
        return 0;
    }
    return -1;
}
