/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueCopyBufferToImageTests;

namespace ULT {

struct clEnqueueCopyBufferToImageTests : public ApiFixture<>,
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

TEST_F(clEnqueueCopyBufferToImageTests, GivenInvalidCmdQueueWhenCopyingBufferToImageThenInvalidCommandQueueErrorIsReturned) {
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueCopyBufferToImage(
        nullptr, //commandQueue
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        0u,      //src_offset
        dstOrigin,
        region,
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueCopyBufferToImageTests, GivenInvalidSrcBufferWhenCopyingBufferToImageThenInvalidMemObjectErrorIsReturned) {
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        0u,      //src_offset
        dstOrigin,
        region,
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueCopyBufferToImageTests, GivenValidParametersWhenCopyingBufferToImageThenSuccessIsReturned) {

    imageFormat.image_channel_order = CL_RGBA;
    cl_mem dstImage = ImageFunctions::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc,
                                                             nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, dstImage);
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        srcBuffer.get(),
        dstImage,
        0u, //src_offset
        dstOrigin,
        region,
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferToImageTests, GivenQueueIncapableWhenCopyingBufferToImageThenInvalidOperationIsReturned) {

    imageFormat.image_channel_order = CL_RGBA;
    cl_mem dstImage = ImageFunctions::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc,
                                                             nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, dstImage);
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL);
    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        srcBuffer.get(),
        dstImage,
        0u, //src_offset
        dstOrigin,
        region,
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

typedef clEnqueueCopyBufferToImageTests clEnqueueCopyBufferToImageYUV;

TEST_F(clEnqueueCopyBufferToImageYUV, GivenValidYuvDstImageWhenCopyingBufferToImageThenSuccessIsReturned) {
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {2, 2, 1};
    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        srcBuffer.get(),
        dstImage,
        0,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferToImageYUV, GivenInvalidOriginAndYuvDstImageWhenCopyingBufferToImageThenInvalidValueErrorIsReturned) {
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {1, 2, 0};
    const size_t region[] = {2, 2, 0};
    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        srcBuffer.get(),
        dstImage,
        0,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferToImageYUV, GivenInvalidRegionAndValidYuvDstImageWhenCopyingBufferToImageThenInvalidValueErrorIsReturned) {
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    auto dstImage = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_ONLY, 0, &imageFormat, &imageDesc, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, dstImage);
    const size_t origin[] = {2, 2, 0};
    const size_t region[] = {1, 2, 0};
    auto retVal = clEnqueueCopyBufferToImage(
        pCommandQueue,
        srcBuffer.get(),
        dstImage,
        0,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(dstImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
