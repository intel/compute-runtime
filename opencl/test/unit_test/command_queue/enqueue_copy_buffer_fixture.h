/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueCopyBufferHelper {
    cl_int enqueueCopyBuffer(
        CommandQueue *pCmdQ,
        Buffer *srcBuffer,
        Buffer *dstBuffer,
        size_t srcOffset,
        size_t dstOffset,
        size_t size,
        cl_uint numEventsInWaitList = 0,
        cl_event *eventWaitList = nullptr,
        cl_event *event = nullptr) {

        cl_int retVal = pCmdQ->enqueueCopyBuffer(
            srcBuffer,
            dstBuffer,
            srcOffset,
            dstOffset,
            size,
            numEventsInWaitList,
            eventWaitList,
            event);
        return retVal;
    }
};

struct EnqueueCopyBufferTest : public CommandEnqueueFixture,
                               public EnqueueCopyBufferHelper,
                               public ::testing::Test {

    void SetUp(void) override {
        CommandEnqueueFixture::SetUp();

        BufferDefaults::context = new MockContext;

        srcBuffer = BufferHelper<>::create();
        dstBuffer = BufferHelper<>::create();
    }

    void TearDown(void) override {
        delete srcBuffer;
        delete dstBuffer;
        delete BufferDefaults::context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    void enqueueCopyBuffer() {
        auto retVal = EnqueueCopyBufferHelper::enqueueCopyBuffer(
            pCmdQ,
            srcBuffer,
            dstBuffer,
            0,
            0,
            sizeof(float),
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    template <typename FamilyType>
    void enqueueCopyBufferAndParse() {
        enqueueCopyBuffer();
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext context;
    Buffer *srcBuffer = nullptr;
    Buffer *dstBuffer = nullptr;
};
} // namespace NEO
