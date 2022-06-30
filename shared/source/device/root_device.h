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

    Device *getRootDevice() const override;
    bool isSubDevice() const override { return false; }

  protected:
    bool createEngines() override;
    void createBindlessHeapsHelper() override;

    void initializeRootCommandStreamReceiver();
};
} // namespace NEO
