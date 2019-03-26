/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueCopyImageToBufferTest : public CommandEnqueueFixture,
                                      public ::testing::Test {

    EnqueueCopyImageToBufferTest() : srcImage(nullptr),
                                     dstBuffer(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext(pDevice);
        context = new MockContext(pDevice);
        srcImage = Image2dHelper<>::create(context);
        dstBuffer = BufferHelper<>::create(context);
    }

    virtual void TearDown(void) override {
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

    MockContext *context;
    Image *srcImage;
    Buffer *dstBuffer;
};

struct EnqueueCopyImageToBufferMipMapTest : public CommandEnqueueFixture,
                                            public ::testing::Test,
                                            public ::testing::WithParamInterface<uint32_t> {

    EnqueueCopyImageToBufferMipMapTest() : dstBuffer(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        BufferDefaults::context = new MockContext(pDevice);
        context = new MockContext(pDevice);
        dstBuffer = BufferHelper<>::create(context);
    }

    virtual void TearDown(void) override {
        delete dstBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

    MockContext *context;
    Buffer *dstBuffer;
};
} // namespace NEO
