/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

template <typename T>
struct clGetImageParams : public ApiFixture<>,
                          public T {

    void SetUp() override {
        ApiFixture::setUp();

        // clang-format off
        imageFormat.image_channel_order     = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = 32;
        imageDesc.image_height      = 32;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object        = nullptr;
        // clang-format on
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

typedef clGetImageParams<::testing::Test> clGetImageParamsTest;

TEST_F(clGetImageParamsTest, GivenValidParamsWhenGettingImageParamsThenSuccessIsReturned) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    cl_int retVal = CL_INVALID_VALUE;

    retVal = clGetImageParamsINTEL(pContext, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(imageRowPitch, 0u);
    EXPECT_NE(imageSlicePitch, 0u);
}

TEST_F(clGetImageParamsTest, GivenDefaultAndSpecializedContextsWhenGettingImageParamsThenTheSameValuesAreReturned) {
    cl_int retVal = CL_INVALID_VALUE;
    MockDefaultContext defaultContext;
    size_t defaultContextImageRowPitch = 0;
    size_t defaultContextImageSlicePitch = 0;

    retVal = clGetImageParamsINTEL(&defaultContext, &imageFormat, &imageDesc,
                                   &defaultContextImageRowPitch, &defaultContextImageSlicePitch);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(defaultContextImageRowPitch, 0u);
    EXPECT_NE(defaultContextImageSlicePitch, 0u);

    MockSpecializedContext specializedContext;
    size_t specializedContextImageRowPitch = 0;
    size_t specializedContextImageSlicePitch = 0;
    retVal = clGetImageParamsINTEL(&specializedContext, &imageFormat, &imageDesc,
                                   &specializedContextImageRowPitch, &specializedContextImageSlicePitch);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(defaultContextImageRowPitch, specializedContextImageRowPitch);
    EXPECT_EQ(defaultContextImageSlicePitch, specializedContextImageSlicePitch);
}

TEST_F(clGetImageParamsTest, GivenNullContextWhenGettingImageParamsThenInvalidContextErrorIsReturned) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    cl_int retVal = CL_SUCCESS;

    retVal = clGetImageParamsINTEL(nullptr, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);
}

TEST_F(clGetImageParamsTest, GivenNullParamsWhenGettingImageParamsThenInvalidValueErrorIsReturned) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    cl_int retVal = CL_SUCCESS;

    retVal = clGetImageParamsINTEL(pContext, nullptr, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);

    retVal = clGetImageParamsINTEL(pContext, &imageFormat, nullptr, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);

    retVal = clGetImageParamsINTEL(pContext, &imageFormat, &imageDesc, nullptr, &imageSlicePitch);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);

    retVal = clGetImageParamsINTEL(pContext, &imageFormat, &imageDesc, &imageRowPitch, nullptr);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);
}

TEST_F(clGetImageParamsTest, GivenInvalidFormatWhenGettingImageParamsThenImageFormatNotSupportedErrorIsReturned) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;

    imageFormat.image_channel_order = CL_A;
    imageFormat.image_channel_data_type = CL_SIGNED_INT32;
    auto retVal = clGetImageParamsINTEL(pContext, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_IMAGE_FORMAT_NOT_SUPPORTED, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);
}
} // namespace ULT
