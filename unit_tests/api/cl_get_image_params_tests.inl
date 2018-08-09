/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/mocks/mock_device.h"

using namespace OCLRT;

namespace ULT {

template <typename T>
struct clGetImageParams : public api_fixture,
                          public T {

    void SetUp() override {
        api_fixture::SetUp();

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
        api_fixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

typedef clGetImageParams<::testing::Test> clGetImageParamsTest;

TEST_F(clGetImageParamsTest, returnsSuccess) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    cl_int retVal = CL_INVALID_VALUE;

    retVal = clGetImageParamsINTEL(pContext, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(imageRowPitch, 0u);
    EXPECT_NE(imageSlicePitch, 0u);
}

TEST_F(clGetImageParamsTest, nullContextReturnsError) {
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    cl_int retVal = CL_SUCCESS;

    retVal = clGetImageParamsINTEL(nullptr, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(imageRowPitch, 0u);
    EXPECT_EQ(imageSlicePitch, 0u);
}

TEST_F(clGetImageParamsTest, nullParamsReturnError) {
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

TEST_F(clGetImageParamsTest, invalidFormatReturnsError) {
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
