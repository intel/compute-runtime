/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/numa_library.h"
#include "shared/source/os_interface/product_helper.h"

#include <algorithm>
#include <iostream>

namespace NEO {

MemoryInfo::MemoryInfo(const RegionContainer &regionInfo, const Drm &inputDrm)
    : drm(inputDrm), drmQueryRegions(regionInfo), systemMemoryRegion(drmQueryRegions[0]) {
    auto ioctlHelper = drm.getIoctlHelper();
    const auto memoryClassSystem = ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem);
    const auto memoryClassDevice = ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice);
    UNRECOVERABLE_IF(this->systemMemoryRegion.region.memoryClass != memoryClassSystem);

    std::ranges::copy_if(drmQueryRegions, std::back_inserter(localMemoryRegions),
                         [memoryClassDevice](const MemoryRegion &memoryRegionInfo) {
                             return (memoryRegionInfo.region.memoryClass == memoryClassDevice);
                         });

    smallBarDetected = std::ranges::any_of(localMemoryRegions,
                                           [](const MemoryRegion &region) {
                                               return (region.cpuVisibleSize && region.cpuVisibleSize < region.probedSize);
                                           });

    populateTileToLocalMemoryRegionIndexMap();

    memPolicySupported = false;
    if (debugManager.flags.EnableHostAllocationMemPolicy.get()) {
        memPolicySupported = Linux::NumaLibrary::init();
    }
    memPolicyMode = debugManager.flags.OverrideHostAllocationMemPolicyMode.get();
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

int MemoryInfo::createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation) {
    std::vector<unsigned long> memPolicyNodeMask;
    int mode = -1;
    auto &productHelper = this->drm.getRootDeviceEnvironment().getHelper<ProductHelper>();
    auto isCoherent = productHelper.isCoherentAllocation(patIndex);
    if (memPolicySupported &&
        isUSMHostAllocation &&
        Linux::NumaLibrary::getMemPolicy(&mode, memPolicyNodeMask)) {
        if (memPolicyMode != -1) {
            mode = memPolicyMode;
        }
        return this->drm.getIoctlHelper()->createGemExt(memClassInstances, allocSize, handle, patIndex, vmId, pairHandle, isChunked, numOfChunks, mode, memPolicyNodeMask, isCoherent);
    } else {
        return this->drm.getIoctlHelper()->createGemExt(memClassInstances, allocSize, handle, patIndex, vmId, pairHandle, isChunked, numOfChunks, std::nullopt, std::nullopt, isCoherent);
    }
}

uint32_t MemoryInfo::getLocalMemoryRegionIndex(DeviceBitfield deviceBitfield) const {
    UNRECOVERABLE_IF(deviceBitfield.count() != 1u);
    auto &hwInfo = *this->drm.getRootDeviceEnvironment().getHardwareInfo();
    auto &gfxCoreHelper = this->drm.getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    auto &productHelper = this->drm.getRootDeviceEnvironment().getHelper<ProductHelper>();
    bool bankOverrideRequired{gfxCoreHelper.isBankOverrideRequired(hwInfo, productHelper)};

    uint32_t tileIndex{bankOverrideRequired ? 0u : Math::log2(static_cast<uint64_t>(deviceBitfield.to_ulong()))};
    if (debugManager.flags.OverrideDrmRegion.get() != -1) {
        tileIndex = debugManager.flags.OverrideDrmRegion.get();
    }
    UNRECOVERABLE_IF(tileIndex >= tileToLocalMemoryRegionIndexMap.size());
    return tileToLocalMemoryRegionIndexMap[tileIndex];
}

uint64_t MemoryInfo::getLocalMemoryRegionSize(uint32_t tileIndex) const {
    UNRECOVERABLE_IF(tileIndex >= tileToLocalMemoryRegionIndexMap.size());
    const auto regionIndex{tileToLocalMemoryRegionIndexMap[tileIndex]};
    return localMemoryRegions[regionIndex].probedSize;
}

MemoryClassInstance MemoryInfo::getMemoryRegionClassAndInstance(DeviceBitfield deviceBitfield, const HardwareInfo &hwInfo) {

    auto &gfxCoreHelper = this->drm.getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.getEnableLocalMemory(hwInfo)) {
        deviceBitfield = 0u;
    }

    return getMemoryRegion(deviceBitfield).region;
}

const MemoryRegion &MemoryInfo::getMemoryRegion(DeviceBitfield deviceBitfield) const {
    if (deviceBitfield.count() == 0) {
        return systemMemoryRegion;
    }

    auto index = getLocalMemoryRegionIndex(deviceBitfield);

    UNRECOVERABLE_IF(index >= localMemoryRegions.size());
    return localMemoryRegions[index];
}

size_t MemoryInfo::getMemoryRegionSize(uint32_t memoryBank) const {
    if (debugManager.flags.PrintMemoryRegionSizes.get()) {
        printRegionSizes();
    }
    return getMemoryRegion(memoryBank).probedSize;
}

void MemoryInfo::printRegionSizes() const {
    for (auto &region : drmQueryRegions) {
        std::cout << "Memory type: " << region.region.memoryClass
                  << ", memory instance: " << region.region.memoryInstance
                  << ", region size: " << region.probedSize << std::endl;
    }
}

int MemoryInfo::createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation) {
    auto pHwInfo = this->drm.getRootDeviceEnvironment().getHardwareInfo();
    auto regionClassAndInstance = getMemoryRegionClassAndInstance(memoryBanks, *pHwInfo);
    MemRegionsVec region = {regionClassAndInstance};
    std::optional<uint32_t> vmId;
    if (!this->drm.isPerContextVMRequired()) {
        if (memoryBanks.count() && debugManager.flags.EnablePrivateBO.get()) {
            auto tileIndex = getLocalMemoryRegionIndex(memoryBanks);
            vmId = this->drm.getVirtualMemoryAddressSpace(tileIndex);
        }
    }
    uint32_t numOfChunks = 0;
    auto ret = createGemExt(region, allocSize, handle, patIndex, vmId, pairHandle, false, numOfChunks, isUSMHostAllocation);
    return ret;
}

int MemoryInfo::createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, bool isUSMHostAllocation) {
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
    uint32_t numOfChunks = 0;
    auto ret = createGemExt(memRegions, allocSize, handle, patIndex, {}, -1, false, numOfChunks, isUSMHostAllocation);
    return ret;
}

int MemoryInfo::createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation) {
    auto pHwInfo = this->drm.getRootDeviceEnvironment().getHardwareInfo();
    MemRegionsVec memRegions{};
    size_t currentBank = 0;
    size_t i = 0;
    while (i < memoryBanks.count()) {
        if (memoryBanks.test(currentBank)) {
            auto regionClassAndInstance = getMemoryRegionClassAndInstance(1u << currentBank, *pHwInfo);
            memRegions.push_back(regionClassAndInstance);
            i++;
        }
        currentBank++;
    }
    auto ret = createGemExt(memRegions, allocSize, handle, patIndex, {}, pairHandle, isChunked, numOfChunks, isUSMHostAllocation);
    return ret;
}

} // namespace NEO
