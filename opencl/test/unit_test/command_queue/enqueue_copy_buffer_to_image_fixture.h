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

struct EnqueueCopyBufferToImageTest : public CommandEnqueueFixture,
                                      public SurfaceStateAccessor,
                                      public ::testing::Test {

    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

        CommandEnqueueFixture::setUp();

        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        srcBuffer = BufferHelper<>::create(context);
        dstImage = Image2dHelperUlt<>::create(context);
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        delete dstImage;
        delete srcBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyBufferToImage() {
        auto retVal = EnqueueCopyBufferToImageHelper<>::enqueueCopyBufferToImage(
            pCmdQ,
            srcBuffer,
            dstImage);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context = nullptr;
    Buffer *srcBuffer = nullptr;
    Image *dstImage = nullptr;
};

struct EnqueueCopyBufferToImageMipMapTest : public CommandEnqueueFixture,
                                            public ::testing::Test,
                                            public ::testing::WithParamInterface<uint32_t> {

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();
        BufferDefaults::context = new MockContext(pClDevice);
        context = new MockContext(pClDevice);
        srcBuffer = BufferHelper<>::create(context);
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete srcBuffer;
        delete BufferDefaults::context;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

    int32_t adjustBuiltInType(bool isHeaplessEnabled, int32_t builtInType) {

        if (isHeaplessEnabled) {
            switch (builtInType) {
            case EBuiltInOps::copyBufferToImage3d:
            case EBuiltInOps::copyBufferToImage3dStateless:
                return EBuiltInOps::copyBufferToImage3dHeapless;
            }
        }

        return builtInType;
    }

    MockContext *context = nullptr;
    Buffer *srcBuffer = nullptr;
};

} // namespace NEO
