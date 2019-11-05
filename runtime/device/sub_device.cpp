/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/sub_device.h"

#include "runtime/device/root_device.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, RootDevice &rootDevice) : Device(executionEnvironment), subDeviceIndex(subDeviceIndex), rootDevice(rootDevice) {}
void SubDevice::retain() {
    rootDevice.incRefInternal();
    Device::retain();
};
unique_ptr_if_unused<Device> SubDevice::release() {
    rootDevice.decRefInternal();
    return Device::release();
};
void SubDevice::retainInternal() {
    rootDevice.incRefInternal();
}
void SubDevice::releaseInternal() {
    rootDevice.decRefInternal();
}

DeviceBitfield SubDevice::getDeviceBitfieldForOsContext() const {
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

} // namespace NEO
