/*
 * Copyright (C) 2019 Intel Corporation
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
    /* We hide the retain and release function of BaseObject. */
    void retain() override;
    unique_ptr_if_unused<Device> release() override;

    uint32_t getNumSubDevices() const;

  protected:
    DeviceBitfield getDeviceBitfield() const override;
    bool createEngines() override;

    MOCKABLE_VIRTUAL bool initializeRootCommandStreamReceiver();
    MOCKABLE_VIRTUAL SubDevice *createSubDevice(uint32_t subDeviceIndex);

    std::vector<std::unique_ptr<SubDevice>> subdevices;
    const uint32_t rootDeviceIndex;
};
} // namespace NEO
