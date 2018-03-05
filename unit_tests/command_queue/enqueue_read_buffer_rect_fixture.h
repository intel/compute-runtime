/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/helpers/aligned_memory.h"
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/mocks/mock_context.h"

namespace OCLRT {

struct EnqueueReadBufferRectTest : public CommandEnqueueFixture,
                                   public ::testing::Test {
    EnqueueReadBufferRectTest(void)
        : buffer(nullptr),
          hostPtr(nullptr) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        context.reset(new MockContext(&pCmdQ->getDevice()));
        BufferDefaults::context = context.get();

        //For 3D
        hostPtr = ::alignedMalloc(slicePitch * rowPitch, 4096);

        auto retVal = CL_INVALID_VALUE;
        buffer.reset(Buffer::create(
            context.get(),
            CL_MEM_READ_WRITE,
            slicePitch * rowPitch,
            nullptr,
            retVal));
        ASSERT_NE(nullptr, buffer.get());

        nonZeroCopyBuffer.reset(BufferHelper<BufferUseHostPtr<>>::create());
    }

    void TearDown() override {
        nonZeroCopyBuffer.reset(nullptr);
        buffer.reset(nullptr);
        ::alignedFree(hostPtr);

        context.reset();
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueReadBufferRect2D(cl_bool blocking = CL_FALSE) {
        typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
        typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

        size_t bufferOrigin[] = {0, 0, 0};
        size_t hostOrigin[] = {0, 0, 0};
        size_t region[] = {50, 50, 1};
        auto retVal = pCmdQ->enqueueReadBufferRect(
            buffer.get(),
            blocking, //non-blocking
            bufferOrigin,
            hostOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            hostPtr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<Buffer> nonZeroCopyBuffer;
    void *hostPtr;

    static const size_t rowPitch = 100;
    static const size_t slicePitch = 100 * 100;
};
} // namespace OCLRT
