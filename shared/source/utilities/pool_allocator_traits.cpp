/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/pool_allocator_traits.h"

#include "shared/source/device/device.h"
#include "shared/source/os_interface/device_factory.h"

namespace NEO {

AllocationProperties TimestampPoolTraits::createAllocationProperties(Device *device, size_t poolSize) {
    return AllocationProperties{device->getRootDeviceIndex(),
                                poolSize,
                                allocationType,
                                device->getDeviceBitfield()};
}

AllocationProperties GlobalSurfacePoolTraits::createAllocationProperties(Device *device, size_t poolSize) {
    return AllocationProperties{device->getRootDeviceIndex(),
                                true, // allocateMemory
                                poolSize,
                                allocationType,
                                false, // isMultiStorageAllocation
                                device->getDeviceBitfield()};
}

AllocationProperties ConstantSurfacePoolTraits::createAllocationProperties(Device *device, size_t poolSize) {
    return AllocationProperties{device->getRootDeviceIndex(),
                                true, // allocateMemory
                                poolSize,
                                allocationType,
                                false, // isMultiStorageAllocation
                                device->getDeviceBitfield()};
}

} // namespace NEO