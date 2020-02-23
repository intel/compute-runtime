/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/linear_stream.h"

#include "fixtures/device_fixture.h"

class DispatcherFixture : public DeviceFixture {
  public:
    void SetUp();
    void TearDown();

    NEO::LinearStream cmdBuffer;
    void *bufferAllocation;
};
