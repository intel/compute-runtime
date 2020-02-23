/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aub_tests/command_stream/aub_command_stream_fixture.h"
#include "command_queue/command_enqueue_fixture.h"
#include "command_queue/command_queue_fixture.h"
#include "command_stream/command_stream_fixture.h"
#include "fixtures/built_in_fixture.h"
#include "fixtures/device_fixture.h"
#include "helpers/hw_parse.h"

namespace NEO {

struct CommandEnqueueAUBFixture : public CommandEnqueueBaseFixture,
                                  public AUBCommandStreamFixture {
    using AUBCommandStreamFixture::SetUp;
    void SetUp() {
        CommandEnqueueBaseFixture::SetUp(cl_command_queue_properties(0));
        AUBCommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() {
        AUBCommandStreamFixture::TearDown();
        CommandEnqueueBaseFixture::TearDown();
    }
};
} // namespace NEO
