/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/surface_formats.h"

using namespace OCLRT;

typedef api_tests clEnqueueCopyImageTests;

namespace ULT {

struct clEnqueueCopyImageTests : public api_fixture,
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

TEST_F(clEnqueueCopyImageTests, nullCommandQueueReturnsError) {
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

TEST_F(clEnqueueCopyImageTests, nullSrcBufferReturnsError) {
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

TEST_F(clEnqueueCopyImageTests, nullDstBufferReturnsError) {
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

TEST_F(clEnqueueCopyImageTests, copyingToInvalidFormatReturnsError) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    imageFormat.image_channel_order = CL_BGRA;
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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

TEST_F(clEnqueueCopyImageTests, returnSuccess) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_WRITE,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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

typedef clEnqueueCopyImageTests clEnqueueCopyImageYUVTests;

TEST_F(clEnqueueCopyImageYUVTests, returnSuccess) {
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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

TEST_F(clEnqueueCopyImageYUVTests, invalidSrcOrigin) {
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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

TEST_F(clEnqueueCopyImageYUVTests, invalidDstOrigin) {
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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

TEST_F(clEnqueueCopyImageYUVTests, invalidDstOriginImage2d) {
    auto srcImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
    auto dstImage = clCreateImage(
        pContext,
        CL_MEM_READ_ONLY,
        &imageFormat,
        &imageDesc,
        nullptr,
        &retVal);
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
