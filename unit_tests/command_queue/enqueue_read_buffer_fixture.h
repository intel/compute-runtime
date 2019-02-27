/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace OCLRT {

struct EnqueueReadBufferTypeTest : public CommandEnqueueFixture,
                                   public ::testing::Test {

    EnqueueReadBufferTypeTest(void)
        : srcBuffer(nullptr) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext;
        srcBuffer.reset(BufferHelper<>::create());
        nonZeroCopyBuffer.reset(BufferHelper<BufferUseHostPtr<>>::create());
    }

    void TearDown() override {
        srcBuffer.reset(nullptr);
        nonZeroCopyBuffer.reset(nullptr);
        delete BufferDefaults::context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueReadBuffer(cl_bool blocking = CL_TRUE) {
        auto retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(
            pCmdQ,
            srcBuffer.get(),
            blocking);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }

    std::unique_ptr<Buffer> srcBuffer;
    std::unique_ptr<Buffer> nonZeroCopyBuffer;
};
} // namespace OCLRT
