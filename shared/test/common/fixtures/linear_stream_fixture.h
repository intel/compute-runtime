/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include <cstdint>

namespace NEO {

struct LinearStreamFixture {
    LinearStreamFixture(void)
        : gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer)), linearStream(&gfxAllocation) {
    }

    virtual void SetUp(void) {
    }

    virtual void TearDown(void) {
    }
    MockGraphicsAllocation gfxAllocation;
    LinearStream linearStream;
    uint32_t pCmdBuffer[1024]{};
};

typedef Test<LinearStreamFixture> LinearStreamTest;
} // namespace NEO
