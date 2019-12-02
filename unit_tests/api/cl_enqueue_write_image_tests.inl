/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/helpers/surface_formats.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueWriteImageTests;

namespace ULT {

struct clEnqueueWriteImageTests : public ApiFixture,
                                  public ::testing::Test {
    void SetUp() override {
        ApiFixture::SetUp();

        // clang-format off
        imageFormat.image_channel_order     = CL_YUYV_INTEL;
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
        ApiFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

TEST_F(clEnqueueWriteImageTests, GivenNullCommandQueueWhenWritingImageThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueWriteImage(
        nullptr,
        nullptr,
        false,
        nullptr,
        nullptr,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueWriteImageTests, GivenNullImageWhenWritingImageThenInvalidMemObjectErrorIsReturned) {
    auto retVal = clEnqueueWriteImage(
        pCommandQueue,
        nullptr,
        false,
        nullptr,
        nullptr,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueWriteImageTests, GivenValidParametersWhenWritingImageThenSuccessIsReturned) {
    imageFormat.image_channel_order = CL_RGBA;
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueWriteImage(
        pCommandQueue,
        image,
        false,
        origin,
        region,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef clEnqueueWriteImageTests clEnqueueWriteImageYUV;

TEST_F(clEnqueueWriteImageYUV, GivenValidParametersWhenWritingYuvImageThenSuccessIsReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueWriteImage(
        pCommandQueue,
        image,
        false,
        origin,
        region,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueWriteImageYUV, GivenInvalidOriginWhenWritingYuvImageThenInvalidValueErrorIsReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {1, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueWriteImage(
        pCommandQueue,
        image,
        false,
        origin,
        region,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
TEST_F(clEnqueueWriteImageYUV, GivenInvalidRegionWhenWritingYuvImageThenInvalidValueErrorIsReturned) {
    auto image = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {1, 2, 0};
    auto retVal = clEnqueueWriteImage(
        pCommandQueue,
        image,
        false,
        origin,
        region,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
