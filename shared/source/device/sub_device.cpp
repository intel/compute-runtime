/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/root_device.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, Device &rootDevice)
    : Device(executionEnvironment, rootDevice.getRootDeviceIndex()), rootDevice(static_cast<RootDevice &>(rootDevice)), subDeviceIndex(subDeviceIndex) {
    UNRECOVERABLE_IF(rootDevice.isSubDevice());
    deviceBitfield = 0;
    deviceBitfield.set(subDeviceIndex);
}

SubDevice::SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, Device &rootDevice, aub_stream::EngineType engineType)
    : SubDevice(executionEnvironment, subDeviceIndex, rootDevice) {
}

void SubDevice::incRefInternal() {
    rootDevice.incRefInternal();
}
unique_ptr_if_unused<Device> SubDevice::decRefInternal() {
    return rootDevice.decRefInternal();
}

uint32_t SubDevice::getSubDeviceIndex() const {
    return subDeviceIndex;
}

Device *SubDevice::getRootDevice() const {
    return &rootDevice;
}

} // namespace NEO
