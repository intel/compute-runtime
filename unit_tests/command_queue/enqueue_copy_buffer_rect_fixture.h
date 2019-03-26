/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueCopyBufferRectHelper {
    cl_int enqueueCopyBufferRect(
        CommandQueue *pCmdQ,
        Buffer *srcBuffer,
        Buffer *dstBuffer,
        const size_t *srcOrigin,
        const size_t *dstOrigin,
        const size_t *region,
        size_t srcRowPitch,
        size_t srcSlicePitch,
        size_t dstRowPitch,
        size_t dstSlicePitch,
        cl_uint numEventsInWaitList,
        const cl_event *eventWaitList,
        cl_event *event) {
        cl_int retVal = pCmdQ->enqueueCopyBufferRect(
            srcBuffer,
            dstBuffer,
            srcOrigin,
            dstOrigin,
            region,
            srcRowPitch,
            srcSlicePitch,
            dstRowPitch,
            dstSlicePitch,
            numEventsInWaitList,
            eventWaitList,
            event);
        return retVal;
    }
};

struct EnqueueCopyBufferRectTest : public CommandEnqueueFixture,
                                   public EnqueueCopyBufferRectHelper,
                                   public ::testing::Test {

    EnqueueCopyBufferRectTest(void) : srcBuffer(nullptr),
                                      dstBuffer(nullptr) {
    }

    struct BufferRect : public BufferDefaults {
        static const size_t sizeInBytes;
    };

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext;

        srcBuffer = BufferHelper<BufferRect>::create();
        dstBuffer = BufferHelper<BufferRect>::create();
    }

    virtual void TearDown(void) override {
        delete srcBuffer;
        delete dstBuffer;
        delete BufferDefaults::context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyBufferRect2D() {
        typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
        typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

        size_t srcOrigin[] = {0, 0, 0};
        size_t dstOrigin[] = {0, 0, 0};
        size_t region[] = {50, 50, 1};
        auto retVal = EnqueueCopyBufferRectHelper::enqueueCopyBufferRect(
            pCmdQ,
            srcBuffer,
            dstBuffer,
            srcOrigin,
            dstOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }

    template <typename FamilyType>
    void enqueueCopyBufferRect3D() {
        typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
        typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

        size_t srcOrigin[] = {0, 0, 0};
        size_t dstOrigin[] = {0, 0, 0};
        size_t region[] = {50, 50, 50};
        auto retVal = EnqueueCopyBufferRectHelper::enqueueCopyBufferRect(
            pCmdQ,
            srcBuffer,
            dstBuffer,
            srcOrigin,
            dstOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    Buffer *srcBuffer;
    Buffer *dstBuffer;

    static const size_t rowPitch = 100;
    static const size_t slicePitch = 100 * 100;
};
} // namespace NEO
