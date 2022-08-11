/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/fixtures/device_fixture.h"

namespace NEO {

class DispatcherFixture : public DeviceFixture {
  public:
    void setUp();
    void tearDown();

    NEO::LinearStream cmdBuffer;
    void *bufferAllocation;
};

} // namespace NEO
