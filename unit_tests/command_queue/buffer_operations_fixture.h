/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
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
