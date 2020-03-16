/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

class PlatformFixture {
  protected:
    void SetUp();
    void TearDown();

    Platform *pPlatform = nullptr;

    cl_uint num_devices = 0u;
    cl_device_id *devices = nullptr;
};
} // namespace NEO
