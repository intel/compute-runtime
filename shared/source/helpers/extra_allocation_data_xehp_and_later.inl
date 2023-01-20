/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const {
    const auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    if (LocalMemoryAccessMode::CpuAccessDisallowed == productHelper.getLocalMemoryAccessMode(hwInfo)) {
        if (properties.allocationType == AllocationType::LINEAR_STREAM ||
            properties.allocationType == AllocationType::INTERNAL_HEAP ||
            properties.allocationType == AllocationType::PRINTF_SURFACE ||
            properties.allocationType == AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER ||
            properties.allocationType == AllocationType::RING_BUFFER ||
            properties.allocationType == AllocationType::SEMAPHORE_BUFFER) {
            allocationData.flags.useSystemMemory = true;
        }
        if (!allocationData.flags.useSystemMemory) {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.storageInfo.isLockable = false;
        }
    } else if (hwInfo.featureTable.flags.ftrLocalMemory &&
               (properties.allocationType == AllocationType::COMMAND_BUFFER ||
                properties.allocationType == AllocationType::RING_BUFFER ||
                properties.allocationType == AllocationType::SEMAPHORE_BUFFER)) {
        allocationData.flags.useSystemMemory = false;
        allocationData.flags.requiresCpuAccess = true;
    }

    if (CompressionSelector::allowStatelessCompression()) {
        if (properties.allocationType == AllocationType::GLOBAL_SURFACE ||
            properties.allocationType == AllocationType::CONSTANT_SURFACE ||
            properties.allocationType == AllocationType::PRINTF_SURFACE) {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.storageInfo.isLockable = false;
        }
    }

    if (productHelper.isStorageInfoAdjustmentRequired()) {
        if (properties.allocationType == AllocationType::BUFFER && !properties.flags.preferCompressed && !properties.flags.shareable) {
            allocationData.storageInfo.isLockable = true;
        }
    }
}
} // namespace NEO
