/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueCopyImageToBufferTests;

namespace ULT {

struct clEnqueueCopyImageToBufferTests : public ApiFixture<>,
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

TEST_F(clEnqueueCopyImageToBufferTests, GivenInvalidQueueWhenCopyingImageToBufferThenInvalidCommandQueueErrorIsReturned) {
    size_t srcOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueCopyImageToBuffer(
        nullptr,
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        srcOrigin,
        region,
        0, //dstOffset
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueCopyImageToBufferTests, GivenInvalidBufferWhenCopyingImageToBufferThenInvalidMemObjectErrorIsReturned) {
    size_t srcOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        srcOrigin,
        region,
        0, //dstOffset
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueCopyImageToBufferTests, GivenValidParametersWhenCopyingImageToBufferThenSuccessIsReturned) {

    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    size_t srcOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        srcImage,
        dstBuffer.get(),
        srcOrigin,
        region,
        0, //dstOffset
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageToBufferTests, GivenQueueIncapableWhenCopyingImageToBufferThenInvalidOperationIsReturned) {
    imageFormat.image_channel_order = CL_RGBA;
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    size_t srcOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL);
    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        srcImage,
        dstBuffer.get(),
        srcOrigin,
        region,
        0, //dstOffset
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef clEnqueueCopyImageToBufferTests clEnqueueCopyImageToBufferYUVTests;

TEST_F(clEnqueueCopyImageToBufferYUVTests, GivenValidParametersWhenCopyingYuvImageToBufferThenSuccessIsReturned) {
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        srcImage,
        dstBuffer.get(),
        origin,
        region,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageToBufferYUVTests, GivenInvalidOriginWhenCopyingYuvImageToBufferThenInvalidValueErrorIsReturned) {
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    const size_t origin[] = {1, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        srcImage,
        dstBuffer.get(),
        origin,
        region,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyImageToBufferYUVTests, GivenInvalidRegionWhenCopyingYuvImageToBufferThenInvalidValueErrorIsReturned) {
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto srcImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, srcImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {1, 2, 0};
    auto retVal = clEnqueueCopyImageToBuffer(
        pCommandQueue,
        srcImage,
        dstBuffer.get(),
        origin,
        region,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(srcImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
