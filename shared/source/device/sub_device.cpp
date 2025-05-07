/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/root_device.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include <iostream>
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

NEO::SubDeviceIdsVec SubDevice::getSubDeviceIdsFromDevice(NEO::Device &device) {

    NEO::SubDeviceIdsVec subDeviceIds;

    if (device.getNumSubDevices() == 0) {
        subDeviceIds.push_back(NEO::SubDevice::getSubDeviceId(device));
    } else {
        for (auto &subDevice : device.getSubDevices()) {
            if (subDevice == nullptr) {
                continue;
            }
            subDeviceIds.push_back(NEO::SubDevice::getSubDeviceId(*subDevice));
        }
    }
    return subDeviceIds;
}

uint32_t SubDevice::getSubDeviceId(NEO::Device &device) {
    if (!device.isSubDevice()) {
        uint32_t deviceBitField = static_cast<uint32_t>(device.getDeviceBitfield().to_ulong());
        if (device.getDeviceBitfield().count() > 1) {
            deviceBitField &= ~deviceBitField + 1;
        }
        return Math::log2(deviceBitField);
    }
    return static_cast<NEO::SubDevice *>(&device)->getSubDeviceIndex();
}

Device *SubDevice::getRootDevice() const {
    return &rootDevice;
}

} // namespace NEO
