/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueFillBufferFixture : public CommandEnqueueFixture {

    void setUp() {
        CommandEnqueueFixture::setUp();

        BufferDefaults::context = new MockContext;

        buffer = BufferHelper<>::create();
    }

    void tearDown() {
        delete buffer;
        delete BufferDefaults::context;

        CommandEnqueueFixture::tearDown();
    }

    template <typename FamilyType>
    void enqueueFillBuffer() {
        auto retVal = EnqueueFillBufferHelper<>::enqueueFillBuffer(
            pCmdQ,
            buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext context;
    Buffer *buffer = nullptr;
};
} // namespace NEO
