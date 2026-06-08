/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef decltype(&Image::redescribe) RedescribeMethod;

class PackedYuvImageTest : public testing::Test {
  public:
    PackedYuvImageTest() {
    }

  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        flags = CL_MEM_READ_ONLY;

        imageDesc.mem_object = nullptr;
        imageDesc.image_array_size = 0;
        imageDesc.image_depth = 1;
        imageDesc.image_height = 13;
        imageDesc.image_width = 16; // Valid values multiple of 2

        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
    }

    void TearDown() override {
    }

    void validateFormat() {
        retVal = Image::validateImageFormat(&imageFormat);
        if (retVal != CL_SUCCESS) {
            return;
        }
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        retVal = Image::validate(
            &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            surfaceFormat, &imageDesc, nullptr);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
};

cl_channel_order packedYuvChannels[] = {CL_YUYV_INTEL, CL_UYVY_INTEL, CL_YVYU_INTEL, CL_VYUY_INTEL};

TEST_F(PackedYuvImageTest, GivenValidPackedYuvImageFormatAndDescriptorWhenCreatingImageThenIsPackYuvImageReturnsTrue) {
    for (auto channel : packedYuvChannels) {
        imageFormat.image_channel_order = channel;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        std::unique_ptr<Image> image(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_TRUE(isPackedYuvImage(&image->getImageFormat()));
    }
}

TEST_F(PackedYuvImageTest, GivenValidPackedYuvImageFormatAndDescriptorWhenValidatingImageFormatThenValidImageIsReturned) {
    for (auto channel : packedYuvChannels) {
        imageFormat.image_channel_order = channel;
        validateFormat();
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST_F(PackedYuvImageTest, GivenInvalidFormatWhenValidatingImageFormatThenInvalidFormatDescriptorErrorIsReturned) {
    imageFormat.image_channel_data_type = CL_SNORM_INT16;
    for (auto channel : packedYuvChannels) {
        imageFormat.image_channel_order = channel;
        validateFormat();
        EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
    }
}

TEST_F(PackedYuvImageTest, GivenInvalidWidthWhenValidatingImageFormatThenInvalidImageDescriptorErrorIsReturned) {
    imageDesc.image_width = 17;
    for (auto channel : packedYuvChannels) {
        imageFormat.image_channel_order = channel;
        validateFormat();
        EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
    }
}

class PackedYUVImageTest : public testing::Test {
  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_YUYV_INTEL;

        imageDesc.mem_object = NULL;
        imageDesc.image_array_size = 0;
        imageDesc.image_depth = 1;
        imageDesc.image_height = 4 * 4;     // Valid values multiple of 4
        imageDesc.image_width = imageWidth; // Valid values multiple of 4

        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;

        flags = CL_MEM_HOST_NO_ACCESS;
    }

    void validateImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        retVal = Image::validate(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                                 surfaceFormat, &imageDesc, nullptr);
    }

    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                             flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }

    const uint32_t imageWidth = 4 * 4;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
};

TEST_F(PackedYUVImageTest, GivenImageViewWhenYUYVIsPassedThenSuccessIsReturned) {
    std::unique_ptr<Image> image{createImageWithFlags(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL)};

    ASSERT_NE(nullptr, image);

    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.image_width = imageWidth / 2; // YUYV image view to RGBA reads two pixel in one read Y0 U0 Y1 V0
    imageDesc.mem_object = image.get();

    std::unique_ptr<Image> imageRGBAFromYUYV{createImageWithFlags(CL_MEM_READ_WRITE)};

    ASSERT_NE(nullptr, imageRGBAFromYUYV);

    EXPECT_EQ(imageRGBAFromYUYV->getImageDesc().image_width, imageDesc.image_width);
}
