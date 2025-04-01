/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/app_resource_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/release_helper/release_helper.h"

#include <bitset>

namespace NEO {

StorageInfo::StorageInfo() = default;

StorageInfo MemoryManager::createStorageInfoFromProperties(const AllocationProperties &properties) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = GraphicsAllocation::isLockable(properties.allocationType) || (properties.makeDeviceBufferLockable && properties.allocationType == AllocationType::buffer);
    if (properties.subDevicesBitfield.count() == 0) {
        return storageInfo;
    }

    const auto *rootDeviceEnv{executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex].get()};

    const auto deviceCount = GfxCoreHelper::getSubDevicesCount(rootDeviceEnv->getHardwareInfo());
    const auto leastOccupiedBank = getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->getLeastOccupiedBank(properties.subDevicesBitfield);
    const auto subDevicesMask = rootDeviceEnv->deviceAffinityMask.getGenericSubDevicesMask().to_ulong();

    const DeviceBitfield allTilesValue(properties.subDevicesBitfield.count() == 1 ? maxNBitValue(deviceCount) & subDevicesMask : properties.subDevicesBitfield);
    DeviceBitfield preferredTile;
    if (properties.subDevicesBitfield.count() == 1) {
        preferredTile = properties.subDevicesBitfield;
    } else {
        UNRECOVERABLE_IF(!properties.subDevicesBitfield.test(leastOccupiedBank));
        preferredTile.set(leastOccupiedBank);
    }

    storageInfo.pageTablesVisibility = allTilesValue;

    storageInfo.subDeviceBitfield = properties.subDevicesBitfield;
    storageInfo.cpuVisibleSegment = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);

    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, properties.allocationType,
                                          sizeof(storageInfo.resourceTag));

    switch (properties.allocationType) {
    case AllocationType::constantSurface:
    case AllocationType::kernelIsa:
    case AllocationType::kernelIsaInternal:
    case AllocationType::debugModuleArea: {
        auto placeAllocOnMultiTile = (properties.subDevicesBitfield.count() != 1);
        if (placeAllocOnMultiTile) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.tileInstanced = true;
        } else {
            storageInfo.cloningOfPageTables = true;
            storageInfo.tileInstanced = false;
        }
    } break;
    case AllocationType::debugContextSaveArea:
    case AllocationType::workPartitionSurface:
        storageInfo.cloningOfPageTables = false;
        storageInfo.tileInstanced = true;
        break;
    case AllocationType::privateSurface:
    case AllocationType::debugSbaTrackingBuffer:
        storageInfo.cloningOfPageTables = false;

        if (properties.subDevicesBitfield.count() == 1) {
            storageInfo.pageTablesVisibility = preferredTile;
        } else {
            storageInfo.tileInstanced = true;
        }
        break;
    case AllocationType::commandBuffer:
    case AllocationType::internalHeap:
    case AllocationType::linearStream:
    case AllocationType::syncBuffer:
        storageInfo.cloningOfPageTables = properties.flags.multiOsContextCapable;
        if (!properties.flags.multiOsContextCapable) {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::scratchSurface:
    case AllocationType::preemption:
    case AllocationType::deferredTasksList:
        if (properties.flags.multiOsContextCapable) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.tileInstanced = true;
        } else {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::gpuTimestampDeviceBuffer:
        storageInfo.cloningOfPageTables = true;
        if (!properties.flags.multiOsContextCapable) {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::buffer:
    case AllocationType::svmGpu: {
        auto colouringPolicy = properties.colouringPolicy;
        auto granularity = properties.colouringGranularity;

        if (debugManager.flags.MultiStoragePolicy.get() != -1) {
            colouringPolicy = static_cast<ColouringPolicy>(debugManager.flags.MultiStoragePolicy.get());
        }

        if (debugManager.flags.MultiStorageGranularity.get() != -1) {
            granularity = debugManager.flags.MultiStorageGranularity.get() * MemoryConstants::kiloByte;
        }

        DEBUG_BREAK_IF(colouringPolicy == ColouringPolicy::deviceCountBased && granularity != MemoryConstants::pageSize64k);

        if (this->supportsMultiStorageResources &&
            properties.multiStorageResource &&
            properties.size >= deviceCount * granularity &&
            properties.subDevicesBitfield.count() != 1u) {
            storageInfo.multiStorage = true;
            storageInfo.colouringPolicy = colouringPolicy;
            storageInfo.colouringGranularity = granularity;
        }

        if (properties.flags.shareable) {
            storageInfo.isLockable = false;
        }
        break;
    }
    default:
        break;
    }

    storageInfo.localOnlyRequired = getLocalOnlyRequired(properties.allocationType,
                                                         rootDeviceEnv->getProductHelper(),
                                                         rootDeviceEnv->getReleaseHelper(),
                                                         properties.flags.preferCompressed);

    if (debugManager.flags.ForceMultiTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceMultiTileAllocPlacement.get()) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.tileInstanced = true;
        }
    }
    if (debugManager.flags.ForceSingleTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceSingleTileAllocPlacement.get()) {
            storageInfo.cloningOfPageTables = true;
            storageInfo.tileInstanced = false;
        }
    }

    storageInfo.memoryBanks = computeStorageInfoMemoryBanks(properties, preferredTile, allTilesValue);

    return storageInfo;
}
uint32_t StorageInfo::getNumBanks() const {
    if (memoryBanks == 0) {
        return 1u;
    }
    return static_cast<uint32_t>(memoryBanks.count());
}

DeviceBitfield MemoryManager::computeStorageInfoMemoryBanks(const AllocationProperties &properties, DeviceBitfield preferredBank, DeviceBitfield allBanks) {
    auto forcedMultiStoragePlacement = debugManager.flags.OverrideMultiStoragePlacement.get();
    auto subDevicesCount = properties.subDevicesBitfield.count();
    const auto deviceCount = GfxCoreHelper::getSubDevicesCount(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getHardwareInfo());

    DeviceBitfield memoryBanks{preferredBank};

    switch (properties.allocationType) {
    case AllocationType::constantSurface:
    case AllocationType::kernelIsa:
    case AllocationType::kernelIsaInternal:
    case AllocationType::debugModuleArea:
        memoryBanks = (subDevicesCount == 1 ? preferredBank : allBanks);
        break;
    case AllocationType::debugContextSaveArea:
    case AllocationType::workPartitionSurface:
        memoryBanks = allBanks;
        break;
    case AllocationType::privateSurface:
    case AllocationType::debugSbaTrackingBuffer:
        memoryBanks = (subDevicesCount == 1 ? preferredBank : allBanks);
        break;
    case AllocationType::commandBuffer:
    case AllocationType::internalHeap:
    case AllocationType::linearStream:
        memoryBanks = preferredBank;
        break;
    case AllocationType::scratchSurface:
    case AllocationType::preemption:
    case AllocationType::deferredTasksList:
        memoryBanks = (properties.flags.multiOsContextCapable ? allBanks : preferredBank);
        break;
    case AllocationType::buffer:
    case AllocationType::svmGpu: {
        auto granularity = properties.colouringGranularity;
        if (debugManager.flags.MultiStorageGranularity.get() != -1) {
            granularity = debugManager.flags.MultiStorageGranularity.get() * MemoryConstants::kiloByte;
        }

        DEBUG_BREAK_IF(properties.colouringPolicy == ColouringPolicy::deviceCountBased && granularity != MemoryConstants::pageSize64k);

        if (this->supportsMultiStorageResources &&
            properties.multiStorageResource &&
            properties.size >= deviceCount * granularity &&
            properties.subDevicesBitfield.count() != 1u) {

            memoryBanks = (forcedMultiStoragePlacement == -1 ? allBanks : forcedMultiStoragePlacement);
        }
        break;
    }
    case AllocationType::unifiedSharedMemory:
        memoryBanks = (forcedMultiStoragePlacement == -1 ? allBanks : forcedMultiStoragePlacement);
        break;
    default:
        break;
    }

    auto forceLocalMemoryForDirectSubmission = true;
    switch (debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.get()) {
    case 0:
        forceLocalMemoryForDirectSubmission = false;
        break;
    case 1:
        forceLocalMemoryForDirectSubmission = properties.flags.multiOsContextCapable;
        break;
    default:
        break;
    }
    if (forceLocalMemoryForDirectSubmission) {
        if (properties.allocationType == AllocationType::commandBuffer ||
            properties.allocationType == AllocationType::ringBuffer ||
            properties.allocationType == AllocationType::semaphoreBuffer) {
            if (properties.flags.multiOsContextCapable) {
                memoryBanks = {};
                for (auto bank = 0u; bank < deviceCount; bank++) {
                    if (allBanks.test(bank)) {
                        memoryBanks.set(bank);
                        break;
                    }
                }
            }
            UNRECOVERABLE_IF(memoryBanks.none());
        }
    }

    if (debugManager.flags.ForceMultiTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceMultiTileAllocPlacement.get()) {
            memoryBanks = allBanks;
        }
    }
    if (debugManager.flags.ForceSingleTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceSingleTileAllocPlacement.get()) {
            memoryBanks = preferredBank;
        }
    }

    return memoryBanks;
}

uint32_t StorageInfo::getTotalBanksCnt() const {
    return Math::log2(getMemoryBanks()) + 1;
}
} // namespace NEO
