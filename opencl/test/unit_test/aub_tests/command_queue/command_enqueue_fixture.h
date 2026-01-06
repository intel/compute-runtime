/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

namespace NEO {
class MockDevice;
struct CommandEnqueueAUBFixture : public AUBFixture {
    void setUp() {
        AUBFixture::setUp(nullptr);
        pDevice = &device->device;
        pClDevice = device;
    }

    void tearDown() {
        AUBFixture::tearDown();
    }
    MockDevice *pDevice = nullptr;
    MockClDevice *pClDevice = nullptr;
};
} // namespace NEO
