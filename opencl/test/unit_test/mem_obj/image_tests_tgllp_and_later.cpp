/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

struct ImageTestsTgllPlus : ClDeviceFixture, testing::Test {
    void SetUp() override {
        ClDeviceFixture::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
        srcImage = std::unique_ptr<Image>(Image3dHelper<>::create(context.get()));
    }

    void TearDown() override {
        srcImage.reset();
        context.reset();
        ClDeviceFixture::TearDown();
    }

    std::unique_ptr<MockContext> context{};
    std::unique_ptr<Image> srcImage{};
};

using TgllpPlusMatcher = IsAtLeastProduct<IGFX_TIGERLAKE_LP>;
HWTEST2_F(ImageTestsTgllPlus, givenDepthResourceWhenSettingImageArgThenSetDepthStencilResourceField, TgllpPlusMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState{};
    auto &gpuFlags = srcImage->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Gpu;

    gpuFlags.Depth = 0;
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
    EXPECT_FALSE(surfaceState.getDepthStencilResource());

    gpuFlags.Depth = 1;
    srcImage->setImageArg(&surfaceState, false, 0, pClDevice->getRootDeviceIndex(), false);
    EXPECT_TRUE(surfaceState.getDepthStencilResource());
}
