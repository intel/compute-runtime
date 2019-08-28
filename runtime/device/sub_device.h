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
    SubDevice(ExecutionEnvironment *executionEnvironment, uint32_t deviceIndex, RootDevice &rootDevice);
    void retain() override;
    unique_ptr_if_unused<Device> release() override;
    void retainInternal();
    void releaseInternal();

  protected:
    RootDevice &rootDevice;
};
} // namespace NEO
