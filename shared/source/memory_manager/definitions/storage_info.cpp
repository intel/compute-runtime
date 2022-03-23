/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    storageInfo.subDeviceBitfield = properties.subDevicesBitfield;
    storageInfo.isLockable = GraphicsAllocation::isLockable(properties.allocationType);
    storageInfo.cpuVisibleSegment = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);

    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, properties.allocationType,
                                          sizeof(storageInfo.resourceTag));

    switch (properties.allocationType) {
    case AllocationType::KERNEL_ISA:
    case AllocationType::KERNEL_ISA_INTERNAL:
    case AllocationType::DEBUG_MODULE_AREA: {
        auto placeIsaOnMultiTile = (properties.subDevicesBitfield.count() != 1);

        if (executionEnvironment.isDebuggingEnabled()) {
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
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
    case AllocationType::WORK_PARTITION_SURFACE:
        storageInfo.cloningOfPageTables = false;
        storageInfo.memoryBanks = allTilesValue;
        storageInfo.tileInstanced = true;
        break;
    case AllocationType::PRIVATE_SURFACE:
        storageInfo.cloningOfPageTables = false;

        if (properties.subDevicesBitfield.count() == 1) {
            storageInfo.memoryBanks = preferredTile;
            storageInfo.pageTablesVisibility = preferredTile;
        } else {
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        }
        break;
    case AllocationType::COMMAND_BUFFER:
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::LINEAR_STREAM:
        storageInfo.cloningOfPageTables = properties.flags.multiOsContextCapable;
        storageInfo.memoryBanks = preferredTile;
        if (!properties.flags.multiOsContextCapable) {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::SCRATCH_SURFACE:
    case AllocationType::PREEMPTION:
        if (properties.flags.multiOsContextCapable) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        } else {
            storageInfo.memoryBanks = preferredTile;
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
        storageInfo.cloningOfPageTables = true;
        if (!properties.flags.multiOsContextCapable) {
            storageInfo.pageTablesVisibility = preferredTile;
        }
        break;
    case AllocationType::BUFFER:
    case AllocationType::SVM_GPU: {
        auto colouringPolicy = properties.colouringPolicy;
        auto granularity = properties.colouringGranularity;

        if (DebugManager.flags.MultiStoragePolicy.get() != -1) {
            colouringPolicy = static_cast<ColouringPolicy>(DebugManager.flags.MultiStoragePolicy.get());
        }

        if (DebugManager.flags.MultiStorageGranularity.get() != -1) {
            granularity = DebugManager.flags.MultiStorageGranularity.get() * MemoryConstants::kiloByte;
        }

        DEBUG_BREAK_IF(colouringPolicy == ColouringPolicy::DeviceCountBased && granularity != MemoryConstants::pageSize64k);

        if (this->supportsMultiStorageResources &&
            properties.multiStorageResource &&
            properties.size >= deviceCount * granularity &&
            properties.subDevicesBitfield.count() != 1u) {
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.multiStorage = true;
            storageInfo.colouringPolicy = colouringPolicy;
            storageInfo.colouringGranularity = granularity;
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

        if (properties.flags.shareable) {
            storageInfo.isLockable = false;
        }
        break;
    }
    case AllocationType::UNIFIED_SHARED_MEMORY:
        storageInfo.memoryBanks = allTilesValue;
        break;
    default:
        break;
    }

    bool forceLocalMemoryForDirectSubmission = true;
    switch (DebugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.get()) {
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
        if (properties.allocationType == AllocationType::COMMAND_BUFFER ||
            properties.allocationType == AllocationType::RING_BUFFER ||
            properties.allocationType == AllocationType::SEMAPHORE_BUFFER) {
            if (properties.flags.multiOsContextCapable) {
                storageInfo.memoryBanks = {};
                for (auto bank = 0u; bank < deviceCount; bank++) {
                    if (allTilesValue.test(bank)) {
                        storageInfo.memoryBanks.set(bank);
                        break;
                    }
                }
            }
            UNRECOVERABLE_IF(storageInfo.memoryBanks.none());
        }
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
