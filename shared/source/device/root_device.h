/*
 * Copyright (C) 2019-2025 Intel Corporation
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

    Device *getRootDevice() const override;
    bool isSubDevice() const override { return false; }
    bool createSingleDeviceEngines();
    bool createRootDeviceEngine(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield);

  protected:
    bool createEngines() override;
    void createBindlessHeapsHelper() override;

    void initializeRootCommandStreamReceiver();
};
} // namespace NEO
