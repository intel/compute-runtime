/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/os_interface/linux/drm_mock_exp.h"

namespace PROD_DG1 {
#undef DRM_IOCTL_I915_GEM_CREATE_EXT
#undef __I915_EXEC_UNKNOWN_FLAGS
#include "third_party/uapi/dg1/drm/i915_drm.h"
} // namespace PROD_DG1

using namespace NEO;

class DrmMockProdDg1 : public DrmMockExp {
  public:
    DrmMockProdDg1(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockProdDg1(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmMockProdDg1(RootDeviceEnvironment &rootDeviceEnvironment, const HardwareInfo *inputHwInfo) : DrmMockExp(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
    }

    void handleQueryItem(drm_i915_query_item *queryItem) override {
        switch (queryItem->query_id) {
        case DRM_I915_QUERY_MEMORY_REGIONS:
            if (queryMemoryRegionInfoSuccessCount == 0) {
                queryItem->length = -EINVAL;
            } else {
                queryMemoryRegionInfoSuccessCount--;
                auto numberOfLocalMemories = 1u;
                auto numberOfRegions = 1u + numberOfLocalMemories;
                int regionInfoSize = sizeof(PROD_DG1::drm_i915_query_memory_regions) + numberOfRegions * sizeof(PROD_DG1::drm_i915_memory_region_info);
                if (queryItem->length == 0) {
                    queryItem->length = regionInfoSize;
                } else {
                    EXPECT_EQ(regionInfoSize, queryItem->length);
                    auto queryMemoryRegionInfo = reinterpret_cast<PROD_DG1::drm_i915_query_memory_regions *>(queryItem->data_ptr);
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

    int handleKernelSpecificRequests(unsigned long request, void *arg) override {
        if (request == DRM_IOCTL_I915_GEM_CREATE_EXT) {
            auto createExtParams = static_cast<drm_i915_gem_create_ext *>(arg);
            if (createExtParams->size == 0) {
                return EINVAL;
            }
            createExtParams->handle = 1u;
            this->createExt = *createExtParams;
            auto setparamRegion = reinterpret_cast<PROD_DG1::drm_i915_gem_create_ext_setparam *>(createExtParams->extensions);
            if (setparamRegion->base.name != I915_GEM_CREATE_EXT_SETPARAM) {
                return EINVAL;
            }
            auto regionParam = reinterpret_cast<PROD_DG1::drm_i915_gem_object_param *>(&setparamRegion->param);
            if (regionParam->param != (I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS)) {
                return EINVAL;
            }
            numRegions = regionParam->size;
            memRegions = *reinterpret_cast<drm_i915_gem_memory_class_instance *>(regionParam->data);
            return gemCreateExtRetVal;
        }
        return -1;
    }
};
