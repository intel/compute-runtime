/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {
class Drm;
struct HardwareInfo;

class MemoryInfo {
  public:
    using RegionContainer = std::vector<MemoryRegion>;

    virtual ~MemoryInfo(){};

    MemoryInfo(const RegionContainer &regionInfo);

    void assignRegionsFromDistances(const std::vector<DistanceInfo> &distances);

    MOCKABLE_VIRTUAL uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle);

    MemoryClassInstance getMemoryRegionClassAndInstance(uint32_t memoryBank, const HardwareInfo &hwInfo);

    MOCKABLE_VIRTUAL size_t getMemoryRegionSize(uint32_t memoryBank);

    void printRegionSizes();

    MOCKABLE_VIRTUAL uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle);

    const RegionContainer &getDrmRegionInfos() const { return drmQueryRegions; }

  protected:
    const RegionContainer drmQueryRegions;

    const MemoryRegion &systemMemoryRegion;

    RegionContainer localMemoryRegions;
};

} // namespace NEO
