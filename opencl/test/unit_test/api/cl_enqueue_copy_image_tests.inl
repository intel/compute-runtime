/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/surface_formats.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueCopyImageTests;

namespace ULT {

struct clEnqueueCopyImageTests : public ApiFixture<>,
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

TEST_F(clEnqueueCopyImageTests, GivenNullCommandQueueWhenCopyingImageThenInvalidCommandQueueErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;

    retVal = clEnqueueCopyImage(
        nullptr,
        buffer,
        buffer,
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueCopyImageTests, GivenNullSrcBufferWhenCopyingImageThenInvalidMemObjectErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;

    retVal = clEnqueueCopyImage(
        pCommandQueue,
        nullptr,
        buffer,
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueCopyImageTests, GivenNullDstBufferWhenCopyingImageThenInvalidMemObjectErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;

    retVal = clEnqueueCopyImage(
        pCommandQueue,
        buffer,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueCopyImageTests, GivenDifferentSrcAndDstImageFormatsWhenCopyingImageThenImageFormatMismatchErrorIsReturned) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    imageFormat.image_channel_order = CL_BGRA;
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_IMAGE_FORMAT_MISMATCH, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageTests, GivenValidParametersWhenCopyingImageThenSuccessIsReturned) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageTests, GivenQueueIncapableWhenCopyingImageThenInvalidOperationIsReturned) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL);
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef clEnqueueCopyImageTests clEnqueueCopyImageYUVTests;

TEST_F(clEnqueueCopyImageYUVTests, GivenValidParametersWhenCopyingYuvImageThenSuccessIsReturned) {
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageYUVTests, GivenInvalidSrcOriginWhenCopyingYuvImageThenInvalidValueErrorIsReturned) {
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t srcOrigin[] = {1, 2, 0};
    const size_t dstOrigin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        srcOrigin,
        dstOrigin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageYUVTests, GivenInvalidDstOriginWhenCopyingYuvImageThenInvalidValueErrorIsReturned) {
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t srcOrigin[] = {2, 2, 0};
    const size_t dstOrigin[] = {1, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        srcOrigin,
        dstOrigin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageYUVTests, GivenInvalidDstOriginFor2dImageWhenCopyingYuvImageThenInvalidValueErrorIsReturned) {
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 1};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyImage(
        pCommandQueue,
        srcImage,
        dstImage,
        origin,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
