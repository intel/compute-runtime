/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/platform/platform.h"

namespace NEO {

struct HardwareInfo;

class PlatformFixture {
  public:
    PlatformFixture();

  protected:
    void SetUp();
    void TearDown();

    Platform *pPlatform;

    cl_uint num_devices;
    cl_device_id *devices;
};
} // namespace NEO
