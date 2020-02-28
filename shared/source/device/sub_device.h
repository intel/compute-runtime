/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"

namespace NEO {
class RootDevice;
class SubDevice : public Device {
  public:
    constexpr static uint32_t unspecifiedSubDeviceIndex = std::numeric_limits<uint32_t>::max();

    SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, RootDevice &rootDevice);
    uint32_t getNumAvailableDevices() const override;
    uint32_t getRootDeviceIndex() const override;
    Device *getDeviceById(uint32_t deviceId) const override;
    DeviceInfo &getMutableDeviceInfo();

    uint32_t getSubDeviceIndex() const;

  protected:
    DeviceBitfield getDeviceBitfield() const override;
    const uint32_t subDeviceIndex;
    RootDevice &rootDevice;
};
} // namespace NEO
