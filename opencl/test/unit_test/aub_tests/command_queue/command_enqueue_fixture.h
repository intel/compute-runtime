/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    using AUBCommandStreamFixture::SetUp;
    void SetUp() override {
        CommandEnqueueBaseFixture::SetUp(cl_command_queue_properties(0));
        AUBCommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        AUBCommandStreamFixture::TearDown();
        CommandEnqueueBaseFixture::TearDown();
    }
};
} // namespace NEO
