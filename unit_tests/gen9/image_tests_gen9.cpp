/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

typedef ::testing::Test gen9ImageTests;

GEN9TEST_F(gen9ImageTests, appendSurfaceStateParamsDoesNothing) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = RENDER_SURFACE_STATE::sInit();
    auto surfaceStateAfter = RENDER_SURFACE_STATE::sInit();
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter);

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}

GEN9TEST_F(gen9ImageTests, WhenGetHostPtrRowOrSlicePitchForMapIsCalledWithMipLevelZeroThenReturnHostPtrRowPitchAndSlicePitch) {
    MockContext context;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&context, &imageDesc));
    auto rowPitch = image->getHostPtrRowPitchForMap(0u);
    auto slicePitch = image->getHostPtrSlicePitchForMap(0u);
    EXPECT_EQ(image->getHostPtrRowPitch(), rowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), slicePitch);
}

GEN9TEST_F(gen9ImageTests, WhenGetHostPtrRowOrSlicePitchForMapIsCalledWithMipLevelNonZeroThenReturnHostPtrRowPitchAndSlicePitch) {
    MockContext context;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&context, &imageDesc));
    auto rowPitch = image->getHostPtrRowPitchForMap(1u);
    auto slicePitch = image->getHostPtrSlicePitchForMap(1u);
    EXPECT_EQ(image->getHostPtrRowPitch(), rowPitch);
    EXPECT_EQ(image->getHostPtrSlicePitch(), slicePitch);
}

GEN9TEST_F(gen9ImageTests, givenImageForGen9WhenClearColorParametersAreSetThenSurfaceStateIsNotModified) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = RENDER_SURFACE_STATE::sInit();
    auto surfaceStateAfter = RENDER_SURFACE_STATE::sInit();
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->setClearColorParams(&surfaceStateAfter, imageHw->getGraphicsAllocation()->gmm);

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}
