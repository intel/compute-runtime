/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "drm/i915_drm.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {

struct MemoryInfoImpl : public MemoryInfo {
    ~MemoryInfoImpl() override = default;

    MemoryInfoImpl(const drm_i915_memory_region_info *regionInfo, size_t count) : regions(regionInfo, regionInfo + count) {
    }

    drm_i915_gem_memory_class_instance getMemoryRegionClassAndInstance(uint32_t memoryBank) {
        auto index = (memoryBank > 0) ? Math::log2(memoryBank) + 1 : 0;
        if (index < regions.size()) {
            return regions[index].region;
        }
        return {invalidMemoryRegion(), invalidMemoryRegion()};
    }

    size_t getMemoryRegionSize(uint32_t memoryBank) {
        auto index = (memoryBank > 0) ? Math::log2(memoryBank) + 1 : 0;
        if (index < regions.size()) {
            return regions[index].probed_size;
        }
        return 0;
    }

    static constexpr uint16_t invalidMemoryRegion() {
        return static_cast<uint16_t>(-1);
    }

    std::vector<drm_i915_memory_region_info> regions;
};

} // namespace NEO
