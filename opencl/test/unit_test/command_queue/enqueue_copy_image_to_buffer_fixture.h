/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

namespace NEO {

struct EnqueueCopyImageToBufferTest : public CommandEnqueueFixture,
                                      public SurfaceStateAccessor,
                                      public ::testing::Test {

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

        CommandEnqueueFixture::setUp();
        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        srcImage = Image2dHelperUlt<>::create(context);
        dstBuffer = BufferHelper<>::create(context);
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete srcImage;
        delete dstBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::tearDown();
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
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();
        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        dstBuffer = BufferHelper<>::create(context);
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete dstBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

    MockContext *context = nullptr;
    Buffer *dstBuffer = nullptr;
};
} // namespace NEO
