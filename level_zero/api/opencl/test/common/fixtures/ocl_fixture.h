/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace NEO {
namespace LEO {
namespace ult {

struct OclFixture : public L0::ult::DeviceFixture {
    void setUp();
    void tearDown();

    Platform *platform = nullptr;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
