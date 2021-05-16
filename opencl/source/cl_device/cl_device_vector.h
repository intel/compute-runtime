/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include "opencl/source/api/cl_types.h"

namespace NEO {
class ClDevice;

class ClDeviceVector : public StackVec<ClDevice *, 1> {
  public:
    ClDeviceVector() = default;
    ClDeviceVector(const ClDeviceVector &) = default;
    ClDeviceVector &operator=(const ClDeviceVector &) = default;
    ClDeviceVector(const cl_device_id *devices,
                   cl_uint numDevices);
    void toDeviceIDs(std::vector<cl_device_id> &devIDs) const;
};

} // namespace NEO
