/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"

#include <vector>

namespace NEO {
class Device;
class DeviceVector : public std::vector<Device *> {
  public:
    DeviceVector() = default;
    DeviceVector(const DeviceVector &) = default;
    DeviceVector &operator=(const DeviceVector &) = default;
    DeviceVector(const cl_device_id *devices,
                 cl_uint numDevices);
    void toDeviceIDs(std::vector<cl_device_id> &devIDs);
};

} // namespace NEO
