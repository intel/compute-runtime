/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"

class DispatcherFixture : public DeviceFixture {
  public:
    void SetUp();
    void TearDown();

    NEO::LinearStream cmdBuffer;
    void *bufferAllocation;
};
