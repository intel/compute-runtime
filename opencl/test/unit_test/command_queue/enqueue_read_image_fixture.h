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
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueReadImageTest : public CommandEnqueueFixture,
                              public SurfaceStateAccessor,
                              public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;
    using CommandQueueHwFixture::pCmdQ;

    void SetUp(void) override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

        CommandEnqueueFixture::setUp();

        context = new MockContext(pClDevice);
        srcImage = Image2dHelperUlt<>::create(context);
        srcAllocation = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
        const auto &imageDesc = srcImage->getImageDesc();
        dstPtr = new float[imageDesc.image_width * imageDesc.image_height];
    }

    void TearDown(void) override {
        if (IsSkipped()) {
            return;
        }
        delete srcImage;
        delete[] dstPtr;
        delete context;
        CommandEnqueueFixture::tearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueReadImage(cl_bool blocking = EnqueueReadImageTraits::blocking) {
        auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ,
                                                                 srcImage,
                                                                 blocking);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    float *dstPtr = nullptr;
    Image *srcImage = nullptr;
    GraphicsAllocation *srcAllocation = nullptr;
    MockContext *context = nullptr;
};

struct EnqueueReadImageMipMapTest : public EnqueueReadImageTest,
                                    public ::testing::WithParamInterface<uint32_t> {
};
} // namespace NEO
