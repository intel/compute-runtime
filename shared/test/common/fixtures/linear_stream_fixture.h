/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstdint>

namespace NEO {

struct LinearStreamFixture {
    LinearStreamFixture(void)
        : gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer)), linearStream(&gfxAllocation) {
    }

    virtual void SetUp(void) { // NOLINT(readability-identifier-naming)
    }

    virtual void TearDown(void) { // NOLINT(readability-identifier-naming)
    }
    MockGraphicsAllocation gfxAllocation;
    LinearStream linearStream;
    uint32_t pCmdBuffer[1024]{};
};

typedef Test<LinearStreamFixture> LinearStreamTest;
} // namespace NEO
