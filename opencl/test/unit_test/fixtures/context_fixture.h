/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

namespace NEO {
class MockContext;

class ContextFixture {
  protected:
    void setUp(cl_uint numDevices, cl_device_id *pDeviceList);
    void tearDown();

    MockContext *pContext = nullptr;
};
} // namespace NEO
