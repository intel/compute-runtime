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
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueCopyImageToBufferTest : public CommandEnqueueFixture,
                                      public ::testing::Test {

    void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        srcImage = Image2dHelper<>::create(context);
        dstBuffer = BufferHelper<>::create(context);
    }

    void TearDown(void) override {
        delete srcImage;
        delete dstBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyImageToBuffer() {
        auto retVal = EnqueueCopyImageToBufferHelper<>::enqueueCopyImageToBuffer(
            pCmdQ,
            srcImage,
            dstBuffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context = nullptr;
    Image *srcImage = nullptr;
    Buffer *dstBuffer = nullptr;
};

struct EnqueueCopyImageToBufferMipMapTest : public CommandEnqueueFixture,
                                            public ::testing::Test,
                                            public ::testing::WithParamInterface<uint32_t> {

    void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        dstBuffer = BufferHelper<>::create(context);
    }

    void TearDown(void) override {
        delete dstBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

    MockContext *context = nullptr;
    Buffer *dstBuffer = nullptr;
};
} // namespace NEO
