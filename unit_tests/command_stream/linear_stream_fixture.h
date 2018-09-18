/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "test.h"
#include "runtime/command_stream/linear_stream.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include <cstdint>

namespace OCLRT {

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
} // namespace OCLRT
