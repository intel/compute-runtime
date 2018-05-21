/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

typedef ::testing::Test gen8ImageTests;

GEN8TEST_F(gen8ImageTests, appendSurfaceStateParamsDoesNothing) {
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

GEN8TEST_F(gen8ImageTests, WhenGetHostPtrRowOrSlicePitchForMapIsCalledWithMipLevelZeroThenReturnWidthTimesBytesPerPixelAndRowPitchTimesHeight) {
    MockContext context;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_depth = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&context, &imageDesc));
    auto rowPitch = image->getHostPtrRowPitchForMap(0u);
    auto slicePitch = image->getHostPtrSlicePitchForMap(0u);
    EXPECT_EQ(4 * imageDesc.image_width, rowPitch);
    EXPECT_EQ(imageDesc.image_height * rowPitch, slicePitch);
}

GEN8TEST_F(gen8ImageTests, WhenGetHostPtrRowOrSlicePitchForMapIsCalledWithMipLevelNonZeroThenReturnScaledWidthTimesBytesPerPixelAndRowPitchTimesScaledHeight) {
    MockContext context;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_depth = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&context, &imageDesc));
    auto rowPitch = image->getHostPtrRowPitchForMap(1u);
    auto slicePitch = image->getHostPtrSlicePitchForMap(1u);
    EXPECT_EQ(4 * (imageDesc.image_width >> 1), rowPitch);
    EXPECT_EQ((imageDesc.image_height >> 1) * rowPitch, slicePitch);
}

GEN8TEST_F(gen8ImageTests, WhenGetHostPtrRowOrSlicePitchForMapIsCalledWithMipLevelNonZeroThenReturnedScaledWidthTimesBytesPerPixelAndRowPitchTimesScaledHeightCannotBeZero) {
    MockContext context;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_depth = 5;
    imageDesc.num_mip_levels = 5;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&context, &imageDesc));
    auto rowPitch = image->getHostPtrRowPitchForMap(4u);
    auto slicePitch = image->getHostPtrSlicePitchForMap(4u);
    EXPECT_EQ(4u, rowPitch);
    EXPECT_EQ(rowPitch, slicePitch);
}
