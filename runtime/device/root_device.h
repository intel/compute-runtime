/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device/device.h"

namespace NEO {

class SubDevice;

class RootDevice : public Device {
  public:
    RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex);
    ~RootDevice() override;
    bool createDeviceImpl() override;
    uint32_t getNumAvailableDevices() const override;
    uint32_t getRootDeviceIndex() const override;
    Device *getDeviceById(uint32_t deviceId) const override;
    bool isReleasable() override;

    uint32_t getNumSubDevices() const;

  protected:
    DeviceBitfield getDeviceBitfield() const override;
    bool createEngines() override;

    void initializeRootCommandStreamReceiver();
    MOCKABLE_VIRTUAL SubDevice *createSubDevice(uint32_t subDeviceIndex);

    std::vector<SubDevice *> subdevices;
    const uint32_t rootDeviceIndex;
};
} // namespace NEO
