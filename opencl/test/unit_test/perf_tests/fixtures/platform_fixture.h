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

struct HardwareInfo;

////////////////////////////////////////////////////////////////////////////////
// CPlatformFixture
//      Used to test the Platform class (and many others)
////////////////////////////////////////////////////////////////////////////////
class PlatformFixture {
  public:
    PlatformFixture();

  protected:
    virtual void SetUp(size_t numDevices, const HardwareInfo **pDevices);
    virtual void TearDown();

    Platform *pPlatform;

    cl_uint num_devices;
    cl_device_id *devices;
};
} // namespace NEO
