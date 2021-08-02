/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/app_resource_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <bitset>

namespace NEO {
StorageInfo MemoryManager::createStorageInfoFromProperties(const AllocationProperties &properties) {
    if (properties.subDevicesBitfield.count() == 0) {
        return {};
    }

    const auto deviceCount = HwHelper::getSubDevicesCount(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getHardwareInfo());
    const auto leastOccupiedBank = getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->getLeastOccupiedBank(properties.subDevicesBitfield);
    const auto subDevicesMask = executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->deviceAffinityMask.getGenericSubDevicesMask().to_ulong();

    const DeviceBitfield allTilesValue(properties.subDevicesBitfield.count() == 1
                                           ? maxNBitValue(deviceCount) & subDevicesMask
                                           : properties.subDevicesBitfield);
    DeviceBitfield preferredTile;
    if (properties.subDevicesBitfield.count() == 1) {
        preferredTile = properties.subDevicesBitfield;
    } else {
        UNRECOVERABLE_IF(!properties.subDevicesBitfield.test(leastOccupiedBank));
        preferredTile.set(leastOccupiedBank);
    }

    StorageInfo storageInfo{preferredTile, allTilesValue};
    storageInfo.isLockable = GraphicsAllocation::isLockable(properties.allocationType);
    storageInfo.cpuVisibleSegment = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);

    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, properties.allocationType,
                                          sizeof(storageInfo.resourceTag));

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
    case GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL:
    case GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA: {
        auto placeIsaOnMultiTile = (properties.subDevicesBitfield.count() != 1);

        if (executionEnvironment.isDebuggingEnabled() &&
            executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->debugger.get()) {
            placeIsaOnMultiTile = false;
        }

        if (DebugManager.flags.MultiTileIsaPlacement.get() != -1) {
            placeIsaOnMultiTile = !!DebugManager.flags.MultiTileIsaPlacement.get();
        }
        if (placeIsaOnMultiTile) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        } else {
            storageInfo.cloningOfPageTables = true;
            storageInfo.memoryBanks = preferredTile;
            storageInfo.tileInstanced = false;
        }
    } break;
    case GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA:
    case GraphicsAllocation::AllocationType::PRIVATE_SURFACE:
    case GraphicsAllocation::AllocationType::WORK_PARTITION_SURFACE:
        storageInfo.cloningOfPageTables = false;
        storageInfo.memoryBanks = allTilesValue;
        storageInfo.tileInstanced = true;
        break;
    case GraphicsAllocation::AllocationType::COMMAND_BUFFER:
    case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
    case GraphicsAllocation::AllocationType::LINEAR_STREAM:
        storageInfo.cloningOfPageTables = properties.flags.multiOsContextCapable;
        storageInfo.memoryBanks = preferredTile;
        if (!properties.flags.multiOsContextCapable) {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case GraphicsAllocation::AllocationType::SCRATCH_SURFACE:
    case GraphicsAllocation::AllocationType::PREEMPTION:
        if (properties.flags.multiOsContextCapable) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        } else {
            storageInfo.memoryBanks = preferredTile;
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case GraphicsAllocation::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
        if (properties.flags.multiOsContextCapable) {
            storageInfo.cloningOfPageTables = true;
        } else {
            storageInfo.pageTablesVisibility = preferredTile;
            storageInfo.cloningOfPageTables = false;
        }
        break;
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
    case GraphicsAllocation::AllocationType::SVM_GPU:
        if (this->supportsMultiStorageResources &&
            properties.multiStorageResource &&
            properties.size >= deviceCount * MemoryConstants::pageSize64k &&
            properties.subDevicesBitfield.count() != 1u) {
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.multiStorage = true;
            if (DebugManager.flags.OverrideMultiStoragePlacement.get() != -1) {
                storageInfo.memoryBanks = DebugManager.flags.OverrideMultiStoragePlacement.get();
            }
        }
        if (properties.flags.readOnlyMultiStorage) {
            storageInfo.readOnlyMultiStorage = true;
            storageInfo.cloningOfPageTables = false;
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        }
        storageInfo.localOnlyRequired = true;
        break;
    case GraphicsAllocation::AllocationType::UNIFIED_SHARED_MEMORY:
        storageInfo.memoryBanks = allTilesValue;
        break;
    default:
        break;
    }
    return storageInfo;
}
uint32_t StorageInfo::getNumBanks() const {
    if (memoryBanks == 0) {
        return 1u;
    }
    return static_cast<uint32_t>(memoryBanks.count());
}
} // namespace NEO
