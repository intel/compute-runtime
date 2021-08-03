/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"

namespace NEO {

template <>
void HwHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const {
    if (LocalMemoryAccessMode::CpuAccessDisallowed == getLocalMemoryAccessMode(hwInfo)) {
        if (properties.allocationType == GraphicsAllocation::AllocationType::LINEAR_STREAM ||
            properties.allocationType == GraphicsAllocation::AllocationType::INTERNAL_HEAP ||
            properties.allocationType == GraphicsAllocation::AllocationType::PRINTF_SURFACE ||
            properties.allocationType == GraphicsAllocation::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER ||
            properties.allocationType == GraphicsAllocation::AllocationType::RING_BUFFER ||
            properties.allocationType == GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER) {
            allocationData.flags.useSystemMemory = true;
        }
        if (!allocationData.flags.useSystemMemory) {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.storageInfo.isLockable = false;
        }
    }
}
} // namespace NEO
