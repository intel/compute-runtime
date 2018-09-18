/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/surface_formats.h"

using namespace OCLRT;

typedef api_tests clEnqueueReadImageTests;

namespace ULT {

struct clEnqueueReadImageTests : public api_fixture,
                                 public ::testing::Test {
    void SetUp() override {
        api_fixture::SetUp();

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
        api_fixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
};

TEST_F(clEnqueueReadImageTests, NullCommandQueue_returnsError) {
    auto retVal = clEnqueueReadImage(
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

TEST_F(clEnqueueReadImageTests, nullImageReturnsError) {
    auto retVal = clEnqueueReadImage(
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

TEST_F(clEnqueueReadImageTests, returnSuccess) {
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
    auto retVal = clEnqueueReadImage(
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

typedef clEnqueueReadImageTests clEnqueueReadImageYUV_;

TEST_F(clEnqueueReadImageYUV_, returnSuccess) {
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
    auto retVal = clEnqueueReadImage(
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

TEST_F(clEnqueueReadImageYUV_, invalidOrigin) {
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
    auto retVal = clEnqueueReadImage(
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

TEST_F(clEnqueueReadImageYUV_, invalidRegion) {
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
    auto retVal = clEnqueueReadImage(
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
