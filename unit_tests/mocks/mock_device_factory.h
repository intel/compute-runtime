/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/device_factory.h"

namespace OCLRT {
class MockDeviceFactory : public DeviceFactory {
  public:
    size_t getNumDevices() { return numDevices; };
};
} // namespace OCLRT
