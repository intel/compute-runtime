/*
 * Copyright (C) 2020-2023 Intel Corporation
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

#include <bitset>

namespace NEO {

StorageInfo::StorageInfo() = default;

StorageInfo MemoryManager::createStorageInfoFromProperties(const AllocationProperties &properties) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = GraphicsAllocation::isLockable(properties.allocationType) || (properties.makeDeviceBufferLockable && properties.allocationType == AllocationType::BUFFER);
    if (properties.subDevicesBitfield.count() == 0) {
        return storageInfo;
    }

    const auto deviceCount = GfxCoreHelper::getSubDevicesCount(executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getHardwareInfo());
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

    storageInfo.memoryBanks = preferredTile;
    storageInfo.pageTablesVisibility = allTilesValue;

    storageInfo.subDeviceBitfield = properties.subDevicesBitfield;
    storageInfo.cpuVisibleSegment = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);

    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, properties.allocationType,
                                          sizeof(storageInfo.resourceTag));

    switch (properties.allocationType) {
    case AllocationType::CONSTANT_SURFACE:
    case AllocationType::KERNEL_ISA:
    case AllocationType::KERNEL_ISA_INTERNAL:
    case AllocationType::DEBUG_MODULE_AREA: {
        auto placeAllocOnMultiTile = (properties.subDevicesBitfield.count() != 1);
        if (placeAllocOnMultiTile) {
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
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
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
    case AllocationType::DEFERRED_TASKS_LIST:
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
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.multiStorage = true;
            storageInfo.colouringPolicy = colouringPolicy;
            storageInfo.colouringGranularity = granularity;
            if (debugManager.flags.OverrideMultiStoragePlacement.get() != -1) {
                storageInfo.memoryBanks = debugManager.flags.OverrideMultiStoragePlacement.get();
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
        if (debugManager.flags.OverrideMultiStoragePlacement.get() != -1) {
            storageInfo.memoryBanks = debugManager.flags.OverrideMultiStoragePlacement.get();
        }
        break;
    default:
        break;
    }

    bool forceLocalMemoryForDirectSubmission = true;
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
    if (debugManager.flags.ForceMultiTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::UNKNOWN);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceMultiTileAllocPlacement.get()) {
            storageInfo.cloningOfPageTables = false;
            storageInfo.memoryBanks = allTilesValue;
            storageInfo.tileInstanced = true;
        }
    }
    if (debugManager.flags.ForceSingleTileAllocPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::UNKNOWN);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceSingleTileAllocPlacement.get()) {
            storageInfo.cloningOfPageTables = true;
            storageInfo.memoryBanks = preferredTile;
            storageInfo.tileInstanced = false;
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

uint32_t StorageInfo::getTotalBanksCnt() const {
    return Math::log2(getMemoryBanks()) + 1;
}
} // namespace NEO
