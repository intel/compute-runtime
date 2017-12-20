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
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/image.h"
#include "runtime/helpers/validators.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

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
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        retVal = Image::validate(&context, flags, surfaceFormat, &imageDesc, nullptr);
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(
        &context,
        flags,
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
