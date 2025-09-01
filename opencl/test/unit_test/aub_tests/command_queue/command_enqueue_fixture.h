/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"

namespace NEO {

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
