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
    SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, RootDevice &rootDevice);
    void incRefInternal() override;
    unique_ptr_if_unused<Device> decRefInternal() override;

    uint32_t getNumAvailableDevices() const override;
    uint32_t getRootDeviceIndex() const override;
    Device *getDeviceById(uint32_t deviceId) const override;
    Device *getParentDevice() const override;

    uint32_t getSubDeviceIndex() const;

  protected:
    DeviceBitfield getDeviceBitfield() const override;
    uint64_t getGlobalMemorySize(uint32_t deviceBitfield) const override;
    const uint32_t subDeviceIndex;
    RootDevice &rootDevice;
};
} // namespace NEO
