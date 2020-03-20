/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"

#include <vector>

namespace NEO {
class ClDevice;

class ClDeviceVector : public std::vector<ClDevice *> {
  public:
    ClDeviceVector() = default;
    ClDeviceVector(const ClDeviceVector &) = default;
    ClDeviceVector &operator=(const ClDeviceVector &) = default;
    ClDeviceVector(const cl_device_id *devices,
                   cl_uint numDevices);
    void toDeviceIDs(std::vector<cl_device_id> &devIDs);
};

} // namespace NEO
