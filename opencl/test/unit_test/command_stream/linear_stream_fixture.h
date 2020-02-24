/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include <cstdint>

namespace NEO {

struct LinearStreamFixture {
    LinearStreamFixture(void)
        : gfxAllocation((void *)pCmdBuffer, sizeof(pCmdBuffer)), linearStream(&gfxAllocation) {
    }

    virtual void SetUp(void) {
    }

    virtual void TearDown(void) {
    }
    MockGraphicsAllocation gfxAllocation;
    LinearStream linearStream;
    uint32_t pCmdBuffer[1024];
};

typedef Test<LinearStreamFixture> LinearStreamTest;
} // namespace NEO
