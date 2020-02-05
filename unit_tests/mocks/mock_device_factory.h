/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/device_factory.h"

namespace NEO {
class MockDeviceFactory : public DeviceFactory {
  public:
    size_t getNumDevices() { return numDevices; };
};
} // namespace NEO
