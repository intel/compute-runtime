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
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"

#include "gtest/gtest.h"

namespace OCLRT {

struct EnqueueCopyImageTest : public CommandEnqueueFixture,
                              public ::testing::Test {

    EnqueueCopyImageTest() : srcImage(nullptr),
                             dstImage(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        context = new MockContext(pDevice);
        srcImage = Image2dHelper<>::create(context);
        dstImage = Image2dHelper<>::create(context);
    }

    virtual void TearDown(void) override {
        delete dstImage;
        delete srcImage;
        delete context;
        CommandEnqueueFixture::TearDown();
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

    MockContext *context;
    Image *srcImage;
    Image *dstImage;
};

struct EnqueueCopyImageMipMapTest : public CommandEnqueueFixture,
                                    public ::testing::Test,
                                    public ::testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {

    void SetUp(void) override {
        CommandEnqueueFixture::SetUp();
        context = new MockContext(pDevice);
    }

    virtual void TearDown(void) override {
        delete context;
        CommandEnqueueFixture::TearDown();
    }

    MockContext *context;
};
} // namespace OCLRT
