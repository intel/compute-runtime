/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/platform/platform.h"

namespace OCLRT {

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
} // namespace OCLRT
