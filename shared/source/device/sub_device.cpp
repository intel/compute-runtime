/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"

#include "shared/source/device/root_device.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, RootDevice &rootDevice)
    : Device(executionEnvironment), subDeviceIndex(subDeviceIndex), rootDevice(rootDevice) {
}

void SubDevice::incRefInternal() {
    rootDevice.incRefInternal();
}
unique_ptr_if_unused<Device> SubDevice::decRefInternal() {
    return rootDevice.decRefInternal();
}

DeviceBitfield SubDevice::getDeviceBitfield() const {
    DeviceBitfield deviceBitfield;
    deviceBitfield.set(subDeviceIndex);
    return deviceBitfield;
}
uint32_t SubDevice::getNumAvailableDevices() const {
    return 1u;
}
uint32_t SubDevice::getRootDeviceIndex() const {
    return this->rootDevice.getRootDeviceIndex();
}

uint32_t SubDevice::getSubDeviceIndex() const {
    return subDeviceIndex;
}

Device *SubDevice::getDeviceById(uint32_t deviceId) const {
    UNRECOVERABLE_IF(deviceId >= getNumAvailableDevices());
    return const_cast<SubDevice *>(this);
}

Device *SubDevice::getParentDevice() const {
    return &rootDevice;
}

uint64_t SubDevice::getGlobalMemorySize(uint32_t deviceBitfield) const {
    auto globalMemorySize = Device::getGlobalMemorySize(static_cast<uint32_t>(maxNBitValue(rootDevice.getNumSubDevices())));
    return globalMemorySize / rootDevice.getNumAvailableDevices();
}

} // namespace NEO
