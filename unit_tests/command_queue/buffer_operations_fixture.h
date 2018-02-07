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
#include "runtime/helpers/ptr_math.h"
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "gtest/gtest.h"

namespace OCLRT {

struct EnqueueWriteBufferTypeTest : public CommandEnqueueFixture,
                                    public ::testing::Test {

    EnqueueWriteBufferTypeTest(void)
        : srcBuffer(nullptr) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext;

        zeroCopyBuffer.reset(BufferHelper<>::create());
        srcBuffer.reset(BufferHelper<BufferUseHostPtr<>>::create());
    }

    void TearDown() override {
        srcBuffer.reset(nullptr);
        zeroCopyBuffer.reset(nullptr);
        delete BufferDefaults::context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueWriteBuffer(cl_bool blocking = EnqueueWriteBufferTraits::blocking) {
        auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
            pCmdQ,
            srcBuffer.get(),
            blocking);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }

    template <typename FamilyType>
    void enqueueWriteBuffer(bool Blocking, void *InputData, int size) {
        auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
            pCmdQ,
            srcBuffer.get(),
            Blocking,
            0,
            size,
            InputData);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    std::unique_ptr<Buffer> srcBuffer;
    std::unique_ptr<Buffer> zeroCopyBuffer;
};
} // namespace OCLRT
