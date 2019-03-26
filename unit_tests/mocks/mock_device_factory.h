/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/device_factory.h"

namespace NEO {
class MockDeviceFactory : public DeviceFactory {
  public:
    size_t getNumDevices() { return numDevices; };
};
} // namespace NEO
