/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "drm/i915_drm.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {
class Drm;
struct HardwareInfo;

class MemoryInfo {
  public:
    using RegionContainer = std::vector<drm_i915_memory_region_info>;

    virtual ~MemoryInfo(){};

    MemoryInfo(const drm_i915_memory_region_info *regionInfo, size_t count);

    void assignRegionsFromDistances(const void *distanceInfosPtr, size_t size);

    MOCKABLE_VIRTUAL uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle);

    drm_i915_gem_memory_class_instance getMemoryRegionClassAndInstance(uint32_t memoryBank, const HardwareInfo &hwInfo);

    MOCKABLE_VIRTUAL size_t getMemoryRegionSize(uint32_t memoryBank);

    void printRegionSizes();

    MOCKABLE_VIRTUAL uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle);

    const RegionContainer &getDrmRegionInfos() const { return drmQueryRegions; }

  protected:
    const RegionContainer drmQueryRegions;

    const drm_i915_memory_region_info &systemMemoryRegion;

    RegionContainer localMemoryRegions;
};

} // namespace NEO
