/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device/device.h"

namespace NEO {
class RootDevice;
class SubDevice : public Device {
  public:
    using Device::Device;

  protected:
    RootDevice *rootDevice = nullptr;
};
} // namespace NEO
