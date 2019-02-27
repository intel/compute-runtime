/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <memory>
using namespace OCLRT;

// Tests for cl_khr_image2d_from_buffer
class ImageFromSubBufferTest : public DeviceFixture, public ::testing::Test {
  public:
    ImageFromSubBufferTest() {}

  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_array_size = 0;
        imageDesc.image_depth = 0;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_height = 128 / 2;
        imageDesc.image_width = 256 / 2;
        imageDesc.num_mip_levels = 0;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_samples = 0;

        size = 128 * 256 * 4;
        hostPtr = alignedMalloc(size, 16);
        ASSERT_NE(nullptr, hostPtr);

        parentBuffer = clCreateBuffer(&context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, size, hostPtr, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);

        const cl_buffer_region region = {size / 2, size / 2};

        subBuffer = clCreateSubBuffer(parentBuffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                      reinterpret_cast<const void *>(&region), &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);

        imageDesc.mem_object = subBuffer;
        ASSERT_NE(nullptr, imageDesc.mem_object);
    }
    void TearDown() override {
        clReleaseMemObject(subBuffer);
        clReleaseMemObject(parentBuffer);
        alignedFree(hostPtr);
    }

    Image *createImage() {
        cl_mem_flags flags = CL_MEM_READ_ONLY;
        auto surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(&context, flags, surfaceFormat, &imageDesc, NULL, retVal);
    }
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    void *hostPtr;
    size_t size;
    cl_mem parentBuffer;
    cl_mem subBuffer;
};

TEST_F(ImageFromSubBufferTest, CreateImage2dFromSubBufferWithOffset) {
    std::unique_ptr<Image> imageFromSubBuffer(createImage());
    EXPECT_NE(nullptr, imageFromSubBuffer);

    SurfaceOffsets surfaceOffsets = {0};
    imageFromSubBuffer->getSurfaceOffsets(surfaceOffsets);

    uint32_t offsetExpected = static_cast<uint32_t>(size) / 2;

    EXPECT_EQ(offsetExpected, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffsetForUVplane);
}

TEST_F(ImageFromSubBufferTest, givenSubbufferWithOffsetGreaterThan4GBWhenImageIsCreatedThenSurfaceOffsetsOffsetHasCorrectValue) {
    Buffer *buffer = castToObject<Buffer>(parentBuffer);
    uint64_t offsetExpected = 0;
    cl_buffer_region region = {0, size / 2};

    if (is64bit) {
        offsetExpected = 8 * GB;
        region = {static_cast<size_t>(offsetExpected), size / 2};
    }

    Buffer *subBufferWithBigOffset = buffer->createSubBuffer(CL_MEM_READ_WRITE, &region, retVal);
    imageDesc.mem_object = subBufferWithBigOffset;

    std::unique_ptr<Image> imageFromSubBuffer(createImage());
    EXPECT_NE(nullptr, imageFromSubBuffer);

    SurfaceOffsets surfaceOffsets = {0};
    imageFromSubBuffer->getSurfaceOffsets(surfaceOffsets);

    EXPECT_EQ(offsetExpected, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffsetForUVplane);
    subBufferWithBigOffset->release();
}
