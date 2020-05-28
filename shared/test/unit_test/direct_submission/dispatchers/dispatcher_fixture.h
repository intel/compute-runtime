/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

class DispatcherFixture : public ClDeviceFixture {
  public:
    void SetUp();
    void TearDown();

    NEO::LinearStream cmdBuffer;
    void *bufferAllocation;
};

} // namespace NEO
