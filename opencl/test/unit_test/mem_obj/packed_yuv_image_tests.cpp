/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/helpers/validators.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef decltype(&Image::redescribe) RedescribeMethod;

class PackedYuvImageTest : public testing::Test,
                           public testing::WithParamInterface<unsigned int> {
  public:
    PackedYuvImageTest() {
    }

  protected:
    void SetUp() override {
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = GetParam();

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
        if (retVal != CL_SUCCESS)
            return;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
        retVal = Image::validate(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), surfaceFormat, &imageDesc, nullptr);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem_flags flags;
};

cl_channel_order packedYuvChannels[] = {CL_YUYV_INTEL, CL_UYVY_INTEL, CL_YVYU_INTEL, CL_VYUY_INTEL};

TEST_P(PackedYuvImageTest, isPackedYuvImageReturnsTrue) {

    flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
    auto image = Image::create(
        &context,
        MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(IsPackedYuvImage(&image->getImageFormat()));
    delete image;
}

TEST_P(PackedYuvImageTest, validPackedYuvImageFormatAndDescriptor) {
    flags = CL_MEM_READ_ONLY;
    validateFormat();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(PackedYuvImageTest, invalidPackedYuvImageFormat) {
    imageFormat.image_channel_data_type = CL_SNORM_INT16;
    flags = CL_MEM_READ_ONLY;
    validateFormat();
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_P(PackedYuvImageTest, invalidPackedYuvImageWidth) {
    imageDesc.image_width = 17;
    flags = CL_MEM_READ_ONLY;
    validateFormat();
    EXPECT_EQ(CL_INVALID_IMAGE_DESCRIPTOR, retVal);
}

INSTANTIATE_TEST_CASE_P(
    PackedYuvImageTests,
    PackedYuvImageTest,
    testing::ValuesIn(packedYuvChannels));
