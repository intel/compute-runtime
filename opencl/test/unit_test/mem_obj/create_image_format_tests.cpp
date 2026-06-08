/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

static const unsigned int testImageDimensions = 32;

template <cl_mem_flags clMemFlags>
class CreateImageFormatTest : public testing::Test {
  public:
    CreateImageFormatTest() : flags(clMemFlags) {
    }

  protected:
    void SetUp() override {
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testImageDimensions;
        imageDesc.image_height = testImageDimensions;
        imageDesc.image_depth = 1;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
    }

    void TearDown() override {
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_mem_flags flags;
};

typedef CreateImageFormatTest<CL_MEM_READ_WRITE> ReadWriteFormatTest;

TEST_F(ReadWriteFormatTest, GivenValidFormatWhenCreatingImageThenImageIsCreated) {
    for (auto &surfaceFormat : SurfaceFormats::surfaceFormats(flags)) {
        retVal = CL_SUCCESS;
        std::unique_ptr<Image> image(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            &surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
    }
}

typedef CreateImageFormatTest<CL_MEM_READ_ONLY> ReadOnlyFormatTest;

TEST_F(ReadOnlyFormatTest, GivenValidReadOnlyFormatWhenCreatingImageThenImageIsCreated) {
    for (auto &surfaceFormat : SurfaceFormats::surfaceFormats(flags)) {
        retVal = CL_SUCCESS;
        std::unique_ptr<Image> image(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            &surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
    }
}

typedef CreateImageFormatTest<CL_MEM_WRITE_ONLY> WriteOnlyFormatTest;

TEST_F(WriteOnlyFormatTest, GivenValidWriteOnlyFormatWhenCreatingImageThenImageIsCreated) {
    for (auto &surfaceFormat : SurfaceFormats::surfaceFormats(flags)) {
        retVal = CL_SUCCESS;
        std::unique_ptr<Image> image(Image::create(
            &context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            &surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
    }
}
