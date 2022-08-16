/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

namespace NEO {

struct EnqueueCopyImageTest : public CommandEnqueueFixture,
                              public ::testing::Test {

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();
        context = new MockContext(pClDevice);
        srcImage = Image2dHelper<>::create(context);
        dstImage = Image2dHelper<>::create(context);
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete dstImage;
        delete srcImage;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueCopyImage() {
        auto retVal = EnqueueCopyImageHelper<>::enqueueCopyImage(
            pCmdQ,
            srcImage,
            dstImage);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    MockContext *context = nullptr;
    Image *srcImage = nullptr;
    Image *dstImage = nullptr;
};

struct EnqueueCopyImageMipMapTest : public CommandEnqueueFixture,
                                    public ::testing::Test,
                                    public ::testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();
        context = new MockContext(pClDevice);
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete context;
        CommandEnqueueFixture::tearDown();
    }

    MockContext *context = nullptr;
};
} // namespace NEO
