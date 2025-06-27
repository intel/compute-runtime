/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct ImageTestsTgllAndLater : ClDeviceFixture, testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        context = std::make_unique<MockContext>(pClDevice);
        srcImage = std::unique_ptr<Image>(Image3dHelperUlt<>::create(context.get()));
    }

    void TearDown() override {
        srcImage.reset();
        context.reset();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockContext> context{};
    std::unique_ptr<Image> srcImage{};
};

using TgllpAndLaterMatcher = IsAtLeastProduct<IGFX_TIGERLAKE_LP>;
HWTEST2_F(ImageTestsTgllAndLater, givenDepthResourceWhenSettingImageArgThenSetDepthStencilResourceField, TgllpAndLaterMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState{};
    auto &gpuFlags = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Gpu;

    gpuFlags.Depth = 0;
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex());
    EXPECT_FALSE(surfaceState.getDepthStencilResource());

    gpuFlags.Depth = 1;
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(surfaceState.getDepthStencilResource());
}
