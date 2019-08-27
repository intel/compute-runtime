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

  protected:
    std::vector<SubDevice *> subdevices;
};
} // namespace NEO
