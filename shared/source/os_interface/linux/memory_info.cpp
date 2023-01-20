/*
 * Copyright (C) 2021-2023 Intel Corporation
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

#include <iostream>

namespace NEO {

MemoryInfo::MemoryInfo(const RegionContainer &regionInfo, const Drm &inputDrm)
    : drm(inputDrm), drmQueryRegions(regionInfo), systemMemoryRegion(drmQueryRegions[0]) {
    auto ioctlHelper = drm.getIoctlHelper();
    const auto memoryClassSystem = ioctlHelper->getDrmParamValue(DrmParam::MemoryClassSystem);
    const auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::MemoryClassDevice);
    UNRECOVERABLE_IF(this->systemMemoryRegion.region.memoryClass != memoryClassSystem);
    std::copy_if(drmQueryRegions.begin(), drmQueryRegions.end(), std::back_inserter(localMemoryRegions),
                 [&](const MemoryRegion &memoryRegionInfo) {
                     return (memoryRegionInfo.region.memoryClass == memoryClassDevice);
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

int MemoryInfo::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId, int32_t pairHandle) {
    return this->drm.getIoctlHelper()->createGemExt(memClassInstances, allocSize, handle, vmId, pairHandle);
}

uint32_t MemoryInfo::getTileIndex(uint32_t memoryBank) {
    auto &hwInfo = *this->drm.getRootDeviceEnvironment().getHardwareInfo();
    auto &gfxCoreHelper = this->drm.getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    auto &productHelper = this->drm.getRootDeviceEnvironment().getHelper<ProductHelper>();

    auto tileIndex = Math::log2(memoryBank);
    tileIndex = gfxCoreHelper.isBankOverrideRequired(hwInfo, productHelper) ? 0 : tileIndex;
    if (DebugManager.flags.OverrideDrmRegion.get() != -1) {
        tileIndex = DebugManager.flags.OverrideDrmRegion.get();
    }
    return tileIndex;
}

MemoryClassInstance MemoryInfo::getMemoryRegionClassAndInstance(uint32_t memoryBank, const HardwareInfo &hwInfo) {

    auto &gfxCoreHelper = this->drm.getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.getEnableLocalMemory(hwInfo) || memoryBank == 0) {
        return systemMemoryRegion.region;
    }

    auto index = getTileIndex(memoryBank);

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

int MemoryInfo::createGemExtWithSingleRegion(uint32_t memoryBanks, size_t allocSize, uint32_t &handle, int32_t pairHandle) {
    auto pHwInfo = this->drm.getRootDeviceEnvironment().getHardwareInfo();
    auto regionClassAndInstance = getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
    MemRegionsVec region = {regionClassAndInstance};
    std::optional<uint32_t> vmId;
    if (!this->drm.isPerContextVMRequired()) {
        if (memoryBanks != 0 && DebugManager.flags.EnablePrivateBO.get()) {
            auto tileIndex = getTileIndex(memoryBanks);
            vmId = this->drm.getVirtualMemoryAddressSpace(tileIndex);
        }
    }
    auto ret = createGemExt(region, allocSize, handle, vmId, pairHandle);
    return ret;
}

int MemoryInfo::createGemExtWithMultipleRegions(uint32_t memoryBanks, size_t allocSize, uint32_t &handle) {
    auto pHwInfo = this->drm.getRootDeviceEnvironment().getHardwareInfo();
    auto banks = std::bitset<4>(memoryBanks);
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
    auto ret = createGemExt(memRegions, allocSize, handle, {}, -1);
    return ret;
}

} // namespace NEO
