/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

typedef ICLFamily::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

struct AppendSurfaceStateParamsTest : public ::testing::Test {
    void SetUp() override {
        surfaceState = ICLFamily::cmdInitRenderSurfaceState;
        EXPECT_EQ(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE, surfaceState.getSampleTapDiscardDisable());

        imageDesc.image_width = 32;
        imageDesc.image_height = 0;
        imageDesc.image_depth = 0;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;

        flags = CL_MEM_READ_WRITE;
    }

    void createImage() {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        EXPECT_NE(nullptr, surfaceFormat);
        image.reset(Image::create(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    std::unique_ptr<Image> image;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
    RENDER_SURFACE_STATE surfaceState;
};

GEN11TEST_F(AppendSurfaceStateParamsTest, givenImageFormatWithoutAlphaChannelWhenAppendSurfaceStateParamsThenTapDiscardConfigDoesntChange) {
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_R;
    createImage();

    auto imageHw = static_cast<ImageHw<ICLFamily> *>(image.get());
    imageHw->appendSurfaceStateParams(&surfaceState);

    bool tapDiscardConfigChanged = RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE != surfaceState.getSampleTapDiscardDisable();
    EXPECT_FALSE(tapDiscardConfigChanged);
}

GEN11TEST_F(AppendSurfaceStateParamsTest, givenImageFormatWithAlphaChannelWhenAppendSurfaceStateParamsThenTapDiscardConfigChanges) {
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_RGBA;
    createImage();

    auto imageHw = static_cast<ImageHw<ICLFamily> *>(image.get());
    imageHw->appendSurfaceStateParams(&surfaceState);

    bool tapDiscardConfigChanged = RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE != surfaceState.getSampleTapDiscardDisable();
    EXPECT_TRUE(tapDiscardConfigChanged);
}

typedef ::testing::Test gen11ImageTests;

GEN11TEST_F(gen11ImageTests, givenImageForGen11WhenClearColorParametersAreSetThenSurfaceStateIsNotModified) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->setClearColorParams(&surfaceStateAfter, imageHw->getGraphicsAllocation()->getDefaultGmm());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}
