/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "gtest/gtest.h"
#include "test_traits_common.h"

using namespace NEO;

class DrmTipMock : public DrmMock {
  public:
    DrmTipMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmTipMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmTipMock(RootDeviceEnvironment &rootDeviceEnvironment, const HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        setupIoctlHelper(inputHwInfo->platform.eProductFamily);
    }

    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    uint32_t queryMemoryRegionInfoSuccessCount = std::numeric_limits<uint32_t>::max();

    //DRM_IOCTL_I915_GEM_CREATE_EXT
    drm_i915_gem_create_ext createExt{};
    drm_i915_gem_memory_class_instance memRegions{};
    uint32_t numRegions = 0;
    int gemCreateExtRetVal = 0;

    //DRM_IOCTL_I915_GEM_MMAP_OFFSET
    __u64 mmapOffsetFlagsReceived = 0;
    __u64 offset = 0;
    int mmapOffsetRetVal = 0;

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "";
    }

    virtual int handleRemainingRequests(unsigned long request, void *arg) override {
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
        } else if (request == DRM_IOCTL_I915_GEM_MMAP_OFFSET) {
            auto mmap_arg = static_cast<drm_i915_gem_mmap_offset *>(arg);
            mmapOffsetFlagsReceived = mmap_arg->flags;
            mmap_arg->offset = offset;
            return mmapOffsetRetVal;
        }
        return handleKernelSpecificRequests(request, arg);
    }

    virtual void handleQueryItem(drm_i915_query_item *queryItem) {
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

    virtual int handleKernelSpecificRequests(unsigned long request, void *arg) {
        if (request == DRM_IOCTL_I915_GEM_CREATE_EXT) {
            auto createExtParams = static_cast<drm_i915_gem_create_ext *>(arg);
            if (createExtParams->size == 0) {
                return EINVAL;
            }
            createExtParams->handle = 1u;
            this->createExt = *createExtParams;
            auto extMemRegions = reinterpret_cast<drm_i915_gem_create_ext_memory_regions *>(createExt.extensions);
            if (extMemRegions->base.name != I915_GEM_CREATE_EXT_MEMORY_REGIONS) {
                return EINVAL;
            }
            this->numRegions = extMemRegions->num_regions;
            this->memRegions = *reinterpret_cast<drm_i915_gem_memory_class_instance *>(extMemRegions->regions);
            if (this->numRegions == 0) {
                return EINVAL;
            }
            if ((this->memRegions.memory_class != I915_MEMORY_CLASS_SYSTEM) && (this->memRegions.memory_class != I915_MEMORY_CLASS_DEVICE)) {
                return EINVAL;
            }
            return gemCreateExtRetVal;
        }
        return -1;
    }
};

struct NonDefaultIoctlsSupported {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if (productFamily == IGFX_DG1) {
            return true;
        }
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::isUsingNonDefaultIoctls;
    }
};
