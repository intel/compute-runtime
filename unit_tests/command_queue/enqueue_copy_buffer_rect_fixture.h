/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "gtest/gtest.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/mocks/mock_context.h"

namespace OCLRT {

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
                                   public MemoryManagementFixture,
                                   public ::testing::Test {

    EnqueueCopyBufferRectTest(void) : srcBuffer(nullptr),
                                      dstBuffer(nullptr) {
    }

    struct BufferRect : public BufferDefaults {
        static const size_t sizeInBytes;
    };

    virtual void SetUp(void) override {
        MemoryManagementFixture::SetUp();
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
        MemoryManagementFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyBufferRect2D() {
        typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
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
        typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
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

    MockContext context;
    Buffer *srcBuffer;
    Buffer *dstBuffer;

    static const size_t rowPitch = 100;
    static const size_t slicePitch = 100 * 100;
};
}
