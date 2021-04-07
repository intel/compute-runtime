/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"

#include "shared/source/device/root_device.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, RootDevice &rootDevice)
    : Device(executionEnvironment), subDeviceIndex(subDeviceIndex), rootDevice(rootDevice) {
    deviceBitfield = 0;
    deviceBitfield.set(subDeviceIndex);
}

void SubDevice::incRefInternal() {
    rootDevice.incRefInternal();
}
unique_ptr_if_unused<Device> SubDevice::decRefInternal() {
    return rootDevice.decRefInternal();
}

uint32_t SubDevice::getRootDeviceIndex() const {
    return this->rootDevice.getRootDeviceIndex();
}

uint32_t SubDevice::getSubDeviceIndex() const {
    return subDeviceIndex;
}

Device *SubDevice::getRootDevice() const {
    return &rootDevice;
}

uint64_t SubDevice::getGlobalMemorySize(uint32_t deviceBitfield) const {
    auto globalMemorySize = Device::getGlobalMemorySize(static_cast<uint32_t>(maxNBitValue(rootDevice.getNumSubDevices())));
    return globalMemorySize / rootDevice.getNumAvailableDevices();
}

} // namespace NEO
