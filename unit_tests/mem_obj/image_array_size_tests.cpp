/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "hw_cmds.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

static const unsigned int testImageDimensions = 17;

class ImageArraySizeTest : public DeviceFixture,
                           public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    ImageArraySizeTest() {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        types = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_type = types;
        imageDesc.image_width = testImageDimensions;
        imageDesc.image_height = testImageDimensions;
        imageDesc.image_depth = 0;
        imageDesc.image_array_size = 10;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
        // clang-format on

        context = new MockContext(pDevice);

        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            imageDesc.mem_object = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR, testImageDimensions, nullptr, nullptr);
        }
    }

    void TearDown() override {
        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            clReleaseMemObject(imageDesc.mem_object);
        }
        delete context;
        DeviceFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

typedef ImageArraySizeTest CreateImageArraySize;

HWTEST_P(CreateImageArraySize, arrayTypes) {

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    if (types == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        EXPECT_TRUE(image->isMemObjZeroCopy());
        auto address = image->getCpuAddress();
        EXPECT_NE(nullptr, address);

    } else if (types == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        EXPECT_FALSE(image->isMemObjZeroCopy());
    }
    ASSERT_EQ(10u, image->getImageDesc().image_array_size);

    delete image;
}

static cl_mem_object_type ArrayImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_CASE_P(
    ImageArraySizeTest_Create,
    CreateImageArraySize,
    testing::ValuesIn(ArrayImageTypes));

typedef ImageArraySizeTest CreateImageNonArraySize;

HWTEST_P(CreateImageNonArraySize, NonArrayTypes) {

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    if (types == CL_MEM_OBJECT_IMAGE2D || types == CL_MEM_OBJECT_IMAGE3D) {
        EXPECT_FALSE(image->isMemObjZeroCopy());
    } else {
        EXPECT_TRUE(image->isMemObjZeroCopy());
        auto address = image->getCpuAddress();
        EXPECT_NE(nullptr, address);
    }
    ASSERT_EQ(0u, image->getImageDesc().image_array_size);

    delete image;
}

static cl_mem_object_type NonArrayImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE3D};

INSTANTIATE_TEST_CASE_P(
    ImageArraySizeTest_Create,
    CreateImageNonArraySize,
    testing::ValuesIn(NonArrayImageTypes));

typedef ImageArraySizeTest CreateImageSize;

HWTEST_P(CreateImageSize, GivenImageTypeAndRegionWhenAskedForHostPtrSizeThenProperSizeIsBeingReturned) {
    size_t region[3] = {100, 200, 300};
    auto rowPitch = 1000;
    auto slicePitch = 4000;
    auto pixelSize = 4;
    auto imageType = GetParam();
    auto size = Image::calculateHostPtrSize(region, rowPitch, slicePitch, pixelSize, imageType);

    if ((imageType == CL_MEM_OBJECT_IMAGE1D) || (imageType == CL_MEM_OBJECT_IMAGE1D_BUFFER)) {
        EXPECT_EQ(region[0] * pixelSize, size);
    } else if (imageType == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_EQ((region[1] - 1) * rowPitch + region[0] * pixelSize, size);
    } else if (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        EXPECT_EQ((region[1] - 1) * slicePitch + region[0] * pixelSize, size);
    } else if ((imageType == CL_MEM_OBJECT_IMAGE3D) || (imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY)) {
        EXPECT_EQ((region[2] - 1) * slicePitch + (region[1] - 1) * rowPitch + region[0] * pixelSize, size);
    } else {
        EXPECT_EQ(0u, size);
    }
}

typedef ImageArraySizeTest CreateImageOffset;

HWTEST_P(CreateImageOffset, GivenImageTypeAndRegionWhenAskedForHostPtrOffsetThenProperOffsetIsBeingReturned) {
    size_t region[3] = {100, 1, 1};
    size_t origin[3] = {0, 0, 0};
    auto rowPitch = 1000;
    auto slicePitch = 0;
    auto pixelSize = 4;
    size_t imageOffset;
    auto imageType = GetParam();
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        Image::calculateHostPtrOffset(&imageOffset, origin, region, rowPitch, slicePitch, imageType, pixelSize);
        EXPECT_EQ(origin[0] * pixelSize, imageOffset);
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        region[1] = 200;
        Image::calculateHostPtrOffset(&imageOffset, origin, region, rowPitch, slicePitch, imageType, pixelSize);
        EXPECT_EQ(origin[1] * rowPitch + origin[0] * pixelSize, imageOffset);
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        slicePitch = 4000;
        Image::calculateHostPtrOffset(&imageOffset, origin, region, rowPitch, slicePitch, imageType, pixelSize);
        EXPECT_EQ(origin[1] * slicePitch + origin[0] * pixelSize, imageOffset);
        break;
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        region[2] = 300;
        Image::calculateHostPtrOffset(&imageOffset, origin, region, rowPitch, slicePitch, imageType, pixelSize);
        EXPECT_EQ(origin[2] * slicePitch + origin[1] * rowPitch + origin[0] * pixelSize, imageOffset);
        break;
    case CL_MEM_OBJECT_BUFFER:
        Image::calculateHostPtrOffset(&imageOffset, origin, region, rowPitch, slicePitch, imageType, pixelSize);
        EXPECT_EQ(0u, imageOffset);
        break;
    }
}

typedef ImageArraySizeTest CheckImageType;

TEST_P(CheckImageType, GivenImageTypeWhenImageTypeIsCheckedThenProperValueIsReturned) {
    auto imageType = GetParam();
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE2D:
        EXPECT_TRUE(Image::isImage2d(imageType));
        EXPECT_TRUE(Image::isImage2dOr2dArray(imageType));
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        EXPECT_FALSE(Image::isImage2d(imageType));
        EXPECT_TRUE(Image::isImage2dOr2dArray(imageType));
        break;
    default:
        EXPECT_FALSE(Image::isImage2d(imageType));
        EXPECT_FALSE(Image::isImage2dOr2dArray(imageType));
        break;
    }
}

static cl_mem_object_type AllImageTypes[] = {
    0, //negative scenario
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_CASE_P(
    ImageArraySizeTest_Create,
    CreateImageSize,
    testing::ValuesIn(AllImageTypes));

static cl_mem_object_type AllImageTypesWithBadOne[] = {
    0, //negative scenario
    CL_MEM_OBJECT_BUFFER,
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_CASE_P(
    ImageArraySizeTest_Create,
    CreateImageOffset,
    testing::ValuesIn(AllImageTypesWithBadOne));
