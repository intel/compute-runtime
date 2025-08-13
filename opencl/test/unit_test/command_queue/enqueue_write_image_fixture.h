/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueWriteImageTest : public CommandEnqueueFixture,
                               public SurfaceStateAccessor,
                               public ::testing::Test {

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        CommandEnqueueFixture::setUp();

        context = new MockContext(pClDevice);
        dstImage = Image2dHelperUlt<>::create(context);
        dstAllocation = dstImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());

        const auto &imageDesc = dstImage->getImageDesc();
        srcPtr = new float[imageDesc.image_width * imageDesc.image_height];
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete dstImage;
        delete[] srcPtr;
        delete context;
        CommandEnqueueFixture::tearDown();
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

    float *srcPtr = nullptr;
    Image *dstImage = nullptr;
    GraphicsAllocation *dstAllocation = nullptr;
    MockContext *context = nullptr;
};

struct EnqueueWriteImageMipMapTest : public EnqueueWriteImageTest,
                                     public ::testing::WithParamInterface<uint32_t> {
};
} // namespace NEO
