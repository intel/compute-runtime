/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/mocks/mock_context.h"

namespace NEO {

struct EnqueueFillBufferFixture : public CommandEnqueueFixture {

    EnqueueFillBufferFixture()
        : buffer(nullptr) {
    }

    virtual void SetUp() {
        CommandEnqueueFixture::SetUp();

        BufferDefaults::context = new MockContext;

        buffer = BufferHelper<>::create();
    }

    virtual void TearDown() {
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
    Buffer *buffer;
};
} // namespace NEO
