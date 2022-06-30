/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct clEnqueueMapImageTests : public ApiFixture<>,
                                public ::testing::Test {

    void SetUp() override {
        ApiFixture::SetUp();

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
        ApiFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

TEST_F(clEnqueueMapImageTests, GivenValidParametersWhenMappingImageThenSuccessIsReturned) {
    auto image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    clEnqueueMapImage(
        pCommandQueue,
        image,
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueMapImageTests, GivenQueueIncapableWhenMappingImageThenInvalidOperationIsReturned) {
    auto image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL);
    clEnqueueMapImage(
        pCommandQueue,
        image,
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct clEnqueueMapImageYUVTests : public ApiFixture<>,
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

TEST_F(clEnqueueMapImageYUVTests, GivenValidYuvImageWhenMappingImageThenSuccessIsReturned) {
    auto image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    clEnqueueMapImage(
        pCommandQueue,
        image,
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        0,
        0,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueMapImageYUVTests, GivenInvalidOriginWhenMappingYuvImageThenInvalidValueErrorIsReturned) {
    auto image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {1, 2, 0};
    const size_t region[] = {2, 2, 0};
    clEnqueueMapImage(
        pCommandQueue,
        image,
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        0,
        0,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueMapImageYUVTests, GivenInvalidRegionWhenMappingYuvImageThenInvalidValueErrorIsReturned) {
    auto image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {1, 2, 0};
    clEnqueueMapImage(
        pCommandQueue,
        image,
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        0,
        0,
        0,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
