/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/root_device.h"

#include "runtime/device/sub_device.h"
#include "runtime/helpers/device_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace NEO {

RootDevice::~RootDevice() = default;

uint32_t RootDevice::getNumSubDevices() const {
    return static_cast<uint32_t>(subdevices.size());
}

uint32_t RootDevice::getRootDeviceIndex() const {
    return this->deviceIndex;
}

uint32_t RootDevice::getNumAvailableDevices() const {
    if (subdevices.empty()) {
        return 1u;
    }
    return getNumSubDevices();
}

Device *RootDevice::getDeviceById(uint32_t deviceId) const {
    UNRECOVERABLE_IF(deviceId >= getNumAvailableDevices());
    if (subdevices.empty()) {
        return const_cast<RootDevice *>(this);
    }
    return subdevices[deviceId].get();
};

RootDevice::RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex) : Device(executionEnvironment, deviceIndex) {}
bool RootDevice::createDeviceImpl() {
    auto numSubDevices = DeviceHelper::getSubDevicesCount(&getHardwareInfo());
    if (numSubDevices == 1) {
        numSubDevices = 0;
    }
    subdevices.reserve(numSubDevices);
    for (auto i = 0u; i < numSubDevices; i++) {

        auto subDevice = Device::create<SubDevice>(executionEnvironment, deviceIndex + i + 1, i, *this);
        if (!subDevice) {
            return false;
        }
        subdevices.push_back(std::unique_ptr<SubDevice>(subDevice));
    }
    auto status = Device::createDeviceImpl();
    if (!status) {
        return status;
    }
    return true;
}

/* We hide the retain and release function of BaseObject. */
void RootDevice::retain() {
    DEBUG_BREAK_IF(!isValid());
}

unique_ptr_if_unused<Device> RootDevice::release() {
    DEBUG_BREAK_IF(!isValid());
    return unique_ptr_if_unused<Device>(this, false);
}
DeviceBitfield RootDevice::getDeviceBitfieldForOsContext() const {
    DeviceBitfield deviceBitfield;
    deviceBitfield.set(deviceIndex);
    return deviceBitfield;
}

bool RootDevice::createEngines() {
    if (!executionEnvironment->initializeRootCommandStreamReceiver(*this)) {
        return Device::createEngines();
    }
    return true;
}

void RootDevice::setupRootEngine(EngineControl engine) {
    if (engines.size() > 0u) {
        return;
    }
    defaultEngineIndex = 0;
    engines.emplace_back(engine);
}
} // namespace NEO
