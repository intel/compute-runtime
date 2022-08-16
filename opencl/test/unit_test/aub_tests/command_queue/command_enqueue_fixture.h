/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

struct CommandEnqueueAUBFixture : public CommandEnqueueBaseFixture,
                                  public AUBCommandStreamFixture {
    using AUBCommandStreamFixture::setUp;
    void setUp() {
        CommandEnqueueBaseFixture::setUp(cl_command_queue_properties(0));
        AUBCommandStreamFixture::setUp(pCmdQ);
    }

    void tearDown() {
        AUBCommandStreamFixture::tearDown();
        CommandEnqueueBaseFixture::tearDown();
    }
};
} // namespace NEO
