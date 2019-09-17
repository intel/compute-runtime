/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/sub_device.h"

#include "runtime/device/root_device.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex, uint32_t subDeviceIndex, RootDevice &rootDevice) : Device(executionEnvironment, deviceIndex), subDeviceIndex(subDeviceIndex), rootDevice(rootDevice) {}
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

} // namespace NEO
