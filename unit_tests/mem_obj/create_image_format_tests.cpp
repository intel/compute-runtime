/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

static const unsigned int testImageDimensions = 32;

template <cl_mem_flags _flags>
class CreateImageFormatTest : public testing::TestWithParam<size_t> {
  public:
    CreateImageFormatTest() : flags(_flags) {
    }

  protected:
    void SetUp() override {
        indexImageFormat = GetParam();

        const SurfaceFormatInfo *surfaceFormatTable = nullptr;
        size_t numSurfaceFormats = 0;

        if ((flags & CL_MEM_READ_ONLY) == CL_MEM_READ_ONLY) {
            surfaceFormatTable = readOnlySurfaceFormats;
            numSurfaceFormats = numReadOnlySurfaceFormats;
        } else if ((flags & CL_MEM_WRITE_ONLY) == CL_MEM_WRITE_ONLY) {
            surfaceFormatTable = writeOnlySurfaceFormats;
            numSurfaceFormats = numWriteOnlySurfaceFormats;
        } else {
            surfaceFormatTable = readWriteSurfaceFormats;
            numSurfaceFormats = numReadWriteSurfaceFormats;
        }
        ASSERT_GT(numSurfaceFormats, indexImageFormat);

        surfaceFormat = (SurfaceFormatInfo *)&surfaceFormatTable[indexImageFormat];
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

    SurfaceFormatInfo *surfaceFormat;
    size_t indexImageFormat;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_mem_flags flags;
};

typedef CreateImageFormatTest<CL_MEM_READ_WRITE> ReadWriteFormatTest;

TEST_P(ReadWriteFormatTest, returnsSuccess) {
    auto image = Image::create(
        &context,
        flags,
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
    testing::Range(zero, numReadWriteSurfaceFormats));

typedef CreateImageFormatTest<CL_MEM_READ_ONLY> ReadOnlyFormatTest;

TEST_P(ReadOnlyFormatTest, returnsSuccess) {
    auto image = Image::create(
        &context,
        flags,
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
    testing::Range(zero, numReadOnlySurfaceFormats));

typedef CreateImageFormatTest<CL_MEM_WRITE_ONLY> WriteOnlyFormatTest;

TEST_P(WriteOnlyFormatTest, returnsSuccess) {
    auto image = Image::create(
        &context,
        flags,
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
    testing::Range(zero, numWriteOnlySurfaceFormats));
