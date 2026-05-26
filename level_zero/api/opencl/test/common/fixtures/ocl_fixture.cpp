/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

namespace NEO {
namespace LEO {
namespace ult {

void OclFixture::setUp() {
    DeviceFixture::setUp();
    platform = new Platform(driverHandle->toHandle());
}

void OclFixture::tearDown() {
    delete platform;
    platform = nullptr;
    DeviceFixture::tearDown();
}

} // namespace ult
} // namespace LEO
} // namespace NEO
