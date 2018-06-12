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
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

namespace OCLRT {

struct EnqueueMapBufferTypeTest : public CommandEnqueueFixture,
                                  public ::testing::Test {

    EnqueueMapBufferTypeTest(void)
        : srcBuffer(nullptr) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext;
        srcBuffer = BufferHelper<>::create();
    }

    void TearDown() override {
        delete srcBuffer;
        delete BufferDefaults::context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueMapBuffer(cl_bool blocking = CL_TRUE) {
        cl_int retVal;
        EnqueueMapBufferHelper<>::Traits::errcodeRet = &retVal;
        auto mappedPointer = EnqueueMapBufferHelper<>::enqueueMapBuffer(
            pCmdQ,
            srcBuffer,
            blocking);
        EXPECT_EQ(CL_SUCCESS, *EnqueueMapBufferHelper<>::Traits::errcodeRet);
        EXPECT_NE(nullptr, mappedPointer);
        EnqueueMapBufferHelper<>::Traits::errcodeRet = nullptr;

        parseCommands<FamilyType>(*pCmdQ);
    }

    Buffer *srcBuffer;
};
} // namespace OCLRT
