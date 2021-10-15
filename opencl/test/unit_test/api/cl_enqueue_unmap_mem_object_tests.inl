/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include <memory>

using namespace NEO;

typedef api_tests clEnqueueUnmapMemObjTests;

TEST_F(clEnqueueUnmapMemObjTests, givenValidAddressWhenUnmappingThenReturnSuccess) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, GivenQueueIncapableWhenUnmappingBufferThenInvalidOperationIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL);
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnCpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnGpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    buffer->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clEnqueueUnmapMemObjTests, GivenInvalidMemObjectTypeWhenUnmappingImageThenInvalidMemObjectIsReturned) {
    MockContext context{};
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{&context, allocation};
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, &buffer, CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    buffer.memObjectType = 0x123456;
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        &buffer,
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct clEnqueueUnmapImageTests : clEnqueueUnmapMemObjTests,
                                  ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        clEnqueueUnmapMemObjTests::SetUp();
        const auto imageType = static_cast<cl_mem_object_type>(GetParam());
        this->image.reset(createImage(imageType));
        EXPECT_NE(nullptr, image.get());
    }

    Image *createImage(cl_mem_object_type type) {
        switch (type) {
        case CL_MEM_OBJECT_IMAGE1D:
            return Image1dHelper<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
            return Image1dBufferHelper<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            return Image1dArrayHelper<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE2D:
            return Image2dHelper<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            return Image2dArrayHelper<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE3D:
            return Image3dHelper<>::create(pContext);
        default:
            return nullptr;
        }
    }

    std::unique_ptr<Image> image;

    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
};

TEST_P(clEnqueueUnmapImageTests, GivenValidParametersWhenUnmappingImageThenSuccessIsReturned) {
    void *mappedImage = clEnqueueMapImage(
        pCommandQueue,
        image.get(),
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

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        image.get(),
        mappedImage,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(clEnqueueUnmapImageTests, GivenQueueIncapableParametersWhenUnmappingImageThenInvalidOperationIsReturned) {
    void *mappedImage = clEnqueueMapImage(
        pCommandQueue,
        image.get(),
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

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL);
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        image.get(),
        mappedImage,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

INSTANTIATE_TEST_SUITE_P(
    clEnqueueUnmapImageTests,
    clEnqueueUnmapImageTests,
    ::testing::Values(
        CL_MEM_OBJECT_IMAGE2D,
        CL_MEM_OBJECT_IMAGE3D,
        CL_MEM_OBJECT_IMAGE2D_ARRAY,
        CL_MEM_OBJECT_IMAGE1D,
        CL_MEM_OBJECT_IMAGE1D_ARRAY,
        CL_MEM_OBJECT_IMAGE1D_BUFFER));
