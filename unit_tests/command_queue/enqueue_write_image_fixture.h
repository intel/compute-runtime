/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"

namespace OCLRT {

struct EnqueueWriteImageTest : public CommandEnqueueFixture,
                               public ::testing::Test {

    EnqueueWriteImageTest() : srcPtr(nullptr),
                              dstImage(nullptr) {
    }

    virtual void SetUp(void) override {
        CommandEnqueueFixture::SetUp();

        context = new MockContext(pDevice);
        dstImage = Image2dHelper<>::create(context);

        const auto &imageDesc = dstImage->getImageDesc();
        srcPtr = new float[imageDesc.image_width * imageDesc.image_height];
    }

    virtual void TearDown(void) override {
        delete dstImage;
        delete[] srcPtr;
        delete context;
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueWriteImage(cl_bool blocking = EnqueueWriteImageTraits::blocking) {
        auto retVal = EnqueueWriteImageHelper<>::enqueueWriteImage(
            pCmdQ,
            dstImage,
            blocking);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    float *srcPtr;
    Image *dstImage;
    MockContext *context;
};

struct EnqueueWriteImageMipMapTest : public EnqueueWriteImageTest,
                                     public ::testing::WithParamInterface<uint32_t> {
};
} // namespace OCLRT
