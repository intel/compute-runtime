/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueFillBufferFixture : public CommandEnqueueFixture {

    void SetUp() override {
        CommandEnqueueFixture::SetUp();

        BufferDefaults::context = new MockContext;

        buffer = BufferHelper<>::create();
    }

    void TearDown() override {
        delete buffer;
        delete BufferDefaults::context;

        CommandEnqueueFixture::TearDown();
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
