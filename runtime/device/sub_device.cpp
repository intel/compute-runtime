/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/sub_device.h"

#include "runtime/device/root_device.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex, RootDevice &rootDevice) : Device(executionEnvironment, deviceIndex), rootDevice(rootDevice) {}
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

} // namespace NEO
