/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_management_fixture.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

static const unsigned int testImageDimensions = 32;

template <cl_mem_flags _flags>
class CreateImageFormatTest : public testing::TestWithParam<size_t> {
  public:
    CreateImageFormatTest() : flags(_flags) {
    }

  protected:
    void SetUp() override {
        indexImageFormat = GetParam();

        ArrayRef<const ClSurfaceFormatInfo>
            surfaceFormatTable = SurfaceFormats::surfaceFormats(flags, defaultHwInfo->capabilityTable.supportsOcl21Features);
        ASSERT_GT(surfaceFormatTable.size(), indexImageFormat);

        surfaceFormat = &surfaceFormatTable[indexImageFormat];
        // clang-format off
        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = testImageDimensions;
        imageDesc.image_height      = testImageDimensions;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
    }

    const ClSurfaceFormatInfo *surfaceFormat;
    size_t indexImageFormat;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_mem_flags flags;
};

typedef CreateImageFormatTest<CL_MEM_READ_WRITE> ReadWriteFormatTest;

TEST_P(ReadWriteFormatTest, GivenValidFormatWhenCreatingImageThenImageIsCreated) {
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    delete image;
}

static const size_t zero = 0;

INSTANTIATE_TEST_CASE_P(
    CreateImage,
    ReadWriteFormatTest,
    testing::Range(zero, SurfaceFormats::readWrite().size()));

typedef CreateImageFormatTest<CL_MEM_READ_ONLY> ReadOnlyFormatTest;

TEST_P(ReadOnlyFormatTest, GivenValidReadOnlyFormatWhenCreatingImageThenImageIsCreated) {
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    delete image;
}

INSTANTIATE_TEST_CASE_P(
    CreateImage,
    ReadOnlyFormatTest,
    testing::Range(zero, SurfaceFormats::readOnly12().size()));

typedef CreateImageFormatTest<CL_MEM_WRITE_ONLY> WriteOnlyFormatTest;

TEST_P(WriteOnlyFormatTest, GivenValidWriteOnlyFormatWhenCreatingImageThenImageIsCreated) {
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    delete image;
}

INSTANTIATE_TEST_CASE_P(
    CreateImage,
    WriteOnlyFormatTest,
    testing::Range(zero, SurfaceFormats::writeOnly().size()));
