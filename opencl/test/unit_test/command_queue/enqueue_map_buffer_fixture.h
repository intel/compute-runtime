/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueMapBufferTypeTest : public CommandEnqueueFixture,
                                  public ::testing::Test {

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

    Buffer *srcBuffer = nullptr;
};
} // namespace NEO
