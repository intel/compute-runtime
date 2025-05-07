/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"

namespace NEO {

using SubDeviceIdsVec = StackVec<uint32_t, 4>;

class RootDevice;
class SubDevice : public Device {
  public:
    SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, Device &rootDevice);
    SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t subDeviceIndex, Device &rootDevice, aub_stream::EngineType engineType);
    void incRefInternal() override;
    unique_ptr_if_unused<Device> decRefInternal() override;

    Device *getRootDevice() const override;

    uint32_t getSubDeviceIndex() const;
    static NEO::SubDeviceIdsVec getSubDeviceIdsFromDevice(NEO::Device &device);
    static uint32_t getSubDeviceId(NEO::Device &device);
    bool isSubDevice() const override { return true; }

  protected:
    RootDevice &rootDevice;
    const uint32_t subDeviceIndex;
};
} // namespace NEO
