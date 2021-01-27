/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"

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
    Device *getParentDevice() const override;
    uint32_t getNumSubDevices() const;
    BindlessHeapsHelper *getBindlessHeapsHelper() const override;

  protected:
    DeviceBitfield getDeviceBitfield() const override;
    bool createEngines() override;

    void initializeRootCommandStreamReceiver();
    MOCKABLE_VIRTUAL SubDevice *createSubDevice(uint32_t subDeviceIndex);

    std::vector<SubDevice *> subdevices;
    const uint32_t rootDeviceIndex;
    DeviceBitfield deviceBitfield = DeviceBitfield{1u};
    uint32_t numSubDevices = 0;
};
} // namespace NEO
