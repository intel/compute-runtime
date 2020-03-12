/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"

#include "gtest/gtest.h"

namespace NEO {

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
    void enqueueWriteBuffer(bool blocking, void *inputData, int size) {
        auto retVal = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(
            pCmdQ,
            srcBuffer.get(),
            blocking,
            0,
            size,
            inputData);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    std::unique_ptr<Buffer> srcBuffer;
    std::unique_ptr<Buffer> zeroCopyBuffer;
};
} // namespace NEO
