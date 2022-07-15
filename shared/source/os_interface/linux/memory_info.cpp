/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/i915.h"

#include <iostream>

namespace NEO {

MemoryInfo::MemoryInfo(const RegionContainer &regionInfo)
    : drmQueryRegions(regionInfo), systemMemoryRegion(drmQueryRegions[0]) {
    UNRECOVERABLE_IF(systemMemoryRegion.region.memoryClass != I915_MEMORY_CLASS_SYSTEM);
    std::copy_if(drmQueryRegions.begin(), drmQueryRegions.end(), std::back_inserter(localMemoryRegions),
                 [](const MemoryRegion &memoryRegionInfo) {
                     return (memoryRegionInfo.region.memoryClass == I915_MEMORY_CLASS_DEVICE);
                 });
}

void MemoryInfo::assignRegionsFromDistances(const std::vector<DistanceInfo> &distances) {
    localMemoryRegions.clear();

    uint32_t memoryRegionCounter = 1;
    uint32_t tile = 0;

    for (size_t i = 0; i < distances.size(); i++) {
        if (i > 0 && distances[i].region.memoryInstance != distances[i - 1].region.memoryInstance) {
            UNRECOVERABLE_IF(distances[i].distance == 0);

            memoryRegionCounter++;
            tile++;
        }

        if ((distances[i].distance != 0) || (localMemoryRegions.size() == (tile + 1))) {
            continue;
        }

        UNRECOVERABLE_IF((drmQueryRegions[memoryRegionCounter].region.memoryClass != distances[i].region.memoryClass) ||
                         (drmQueryRegions[memoryRegionCounter].region.memoryInstance != distances[i].region.memoryInstance));

        localMemoryRegions.push_back(drmQueryRegions[memoryRegionCounter]);
    }
}

uint32_t MemoryInfo::createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) {
    return drm->getIoctlHelper()->createGemExt(memClassInstances, allocSize, handle, vmId);
}

uint32_t MemoryInfo::getTileIndex(uint32_t memoryBank, const HardwareInfo &hwInfo) {
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto tileIndex = Math::log2(memoryBank);
    tileIndex = hwHelper.isBankOverrideRequired(hwInfo) ? 0 : tileIndex;
    if (DebugManager.flags.OverrideDrmRegion.get() != -1) {
        tileIndex = DebugManager.flags.OverrideDrmRegion.get();
    }
    return tileIndex;
}

MemoryClassInstance MemoryInfo::getMemoryRegionClassAndInstance(uint32_t memoryBank, const HardwareInfo &hwInfo) {
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (!hwHelper.getEnableLocalMemory(hwInfo) || memoryBank == 0) {
        return systemMemoryRegion.region;
    }

    auto index = getTileIndex(memoryBank, hwInfo);

    UNRECOVERABLE_IF(index >= localMemoryRegions.size());

    return localMemoryRegions[index].region;
}

size_t MemoryInfo::getMemoryRegionSize(uint32_t memoryBank) {
    if (DebugManager.flags.PrintMemoryRegionSizes.get()) {
        printRegionSizes();
    }
    if (memoryBank == 0) {
        return systemMemoryRegion.probedSize;
    }

    auto index = Math::log2(memoryBank);

    if (index < localMemoryRegions.size()) {
        return localMemoryRegions[index].probedSize;
    }

    return 0;
}

void MemoryInfo::printRegionSizes() {
    for (auto region : drmQueryRegions) {
        std::cout << "Memory type: " << region.region.memoryClass
                  << ", memory instance: " << region.region.memoryInstance
                  << ", region size: " << region.probedSize << std::endl;
    }
}

uint32_t MemoryInfo::createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) {
    auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto regionClassAndInstance = getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
    MemRegionsVec region = {regionClassAndInstance};
    std::optional<uint32_t> vmId;
    if (!drm->isPerContextVMRequired()) {
        if (memoryBanks != 0 && DebugManager.flags.EnablePrivateBO.get()) {
            auto tileIndex = getTileIndex(memoryBanks, *pHwInfo);
            vmId = drm->getVirtualMemoryAddressSpace(tileIndex);
        }
    }
    auto ret = createGemExt(drm, region, allocSize, handle, vmId);
    return ret;
}

uint32_t MemoryInfo::createGemExtWithMultipleRegions(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) {
    auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto banks = std::bitset<32>(memoryBanks);
    MemRegionsVec memRegions{};
    size_t currentBank = 0;
    size_t i = 0;
    while (i < banks.count()) {
        if (banks.test(currentBank)) {
            auto regionClassAndInstance = getMemoryRegionClassAndInstance(1u << currentBank, *pHwInfo);
            memRegions.push_back(regionClassAndInstance);
            i++;
        }
        currentBank++;
    }
    auto ret = createGemExt(drm, memRegions, allocSize, handle, {});
    return ret;
}

} // namespace NEO
