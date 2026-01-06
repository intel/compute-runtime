/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/platform/platform.h"

namespace NEO {
class Platform;

class PlatformFixture {
  protected:
    void setUp();
    void tearDown();

    DebugManagerStateRestore restorer;
    Platform *pPlatform = nullptr;

    cl_uint numDevices = 0u;
    cl_device_id *devices = nullptr;
};
} // namespace NEO
