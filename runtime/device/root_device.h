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
    RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex);
    ~RootDevice() override;
    bool createDeviceImpl() override;
    uint32_t getNumSubDevices() const;

    /* We hide the retain and release function of BaseObject. */
    void retain() override;
    unique_ptr_if_unused<Device> release() override;

  protected:
    DeviceBitfield getDeviceBitfieldForOsContext() const override;
    std::vector<std::unique_ptr<SubDevice>> subdevices;
};
} // namespace NEO
