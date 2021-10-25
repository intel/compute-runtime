/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "drm/i915_drm.h"

#include <cstddef>
#include <vector>

namespace NEO {

class MemoryInfoImpl : public MemoryInfo {
  public:
    using RegionContainer = std::vector<drm_i915_memory_region_info>;

    ~MemoryInfoImpl() override = default;

    MemoryInfoImpl(const drm_i915_memory_region_info *regionInfo, size_t count);

    void assignRegionsFromDistances(const void *distanceInfosPtr, size_t size);

    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;

    drm_i915_gem_memory_class_instance getMemoryRegionClassAndInstance(uint32_t memoryBank, const HardwareInfo &hwInfo) {
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (!hwHelper.getEnableLocalMemory(hwInfo) || memoryBank == 0) {
            return systemMemoryRegion.region;
        }

        auto index = Math::log2(memoryBank);

        index = hwHelper.isBankOverrideRequired(hwInfo) ? 0 : index;

        if (DebugManager.flags.OverrideDrmRegion.get() != -1) {
            index = DebugManager.flags.OverrideDrmRegion.get();
        }

        UNRECOVERABLE_IF(index >= localMemoryRegions.size());

        return localMemoryRegions[index].region;
    }

    size_t getMemoryRegionSize(uint32_t memoryBank) override {
        if (DebugManager.flags.PrintMemoryRegionSizes.get()) {
            printRegionSizes();
        }
        if (memoryBank == 0) {
            return systemMemoryRegion.probed_size;
        }

        auto index = Math::log2(memoryBank);

        if (index < localMemoryRegions.size()) {
            return localMemoryRegions[index].probed_size;
        }

        return 0;
    }

    void printRegionSizes() {
        for (auto region : drmQueryRegions) {
            std::cout << "Memory type: " << region.region.memory_class
                      << ", memory instance: " << region.region.memory_instance
                      << ", region size: " << region.probed_size << std::endl;
        }
    }

    uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) override {
        auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
        auto regionClassAndInstance = getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
        auto ret = createGemExt(drm, &regionClassAndInstance, 1, allocSize, handle);
        return ret;
    }

    const RegionContainer &getDrmRegionInfos() const { return drmQueryRegions; }

  protected:
    const RegionContainer drmQueryRegions;

    const drm_i915_memory_region_info &systemMemoryRegion;

    RegionContainer localMemoryRegions;
};

} // namespace NEO
