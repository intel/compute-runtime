/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

static const unsigned int testImageDimensions = 17;

class ImageArraySizeTest : public ClDeviceFixture,
                           public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    ImageArraySizeTest() {
    }

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
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

        context = new MockContext(pClDevice);

        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            imageDesc.mem_object = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR, testImageDimensions, nullptr, nullptr);
        }
    }

    void TearDown() override {
        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            clReleaseMemObject(imageDesc.mem_object);
        }
        delete context;
        ClDeviceFixture::tearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

typedef ImageArraySizeTest CreateImageArraySize;

HWTEST_P(CreateImageArraySize, GivenArrayTypeWhenCreatingImageThenImageCreatedWithCorrectParams) {

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
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
        EXPECT_EQ(!defaultHwInfo->capabilityTable.supportsImages, image->isMemObjZeroCopy());
    }
    ASSERT_EQ(10u, image->getImageDesc().image_array_size);

    delete image;
}

static cl_mem_object_type arrayImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_SUITE_P(
    ImageArraySizeTestCreate,
    CreateImageArraySize,
    testing::ValuesIn(arrayImageTypes));

typedef ImageArraySizeTest CreateImageNonArraySize;

HWTEST_P(CreateImageNonArraySize, GivenNonArrayTypeWhenCreatingImageThenImageCreatedWithCorrectParams) {

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    if (types == CL_MEM_OBJECT_IMAGE2D || types == CL_MEM_OBJECT_IMAGE3D) {
        EXPECT_EQ(!defaultHwInfo->capabilityTable.supportsImages, image->isMemObjZeroCopy());
    } else {
        EXPECT_TRUE(image->isMemObjZeroCopy());
        auto address = image->getCpuAddress();
        EXPECT_NE(nullptr, address);
    }
    ASSERT_EQ(0u, image->getImageDesc().image_array_size);

    delete image;
}

static cl_mem_object_type nonArrayImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE3D};

INSTANTIATE_TEST_SUITE_P(
    ImageArraySizeTest_Create,
    CreateImageNonArraySize,
    testing::ValuesIn(nonArrayImageTypes));

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

static cl_mem_object_type allImageTypes[] = {
    0, // negative scenario
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_SUITE_P(
    ImageArraySizeTest_Create,
    CreateImageSize,
    testing::ValuesIn(allImageTypes));

INSTANTIATE_TEST_SUITE_P(
    ,
    CheckImageType,
    testing::ValuesIn(allImageTypes));

static cl_mem_object_type allImageTypesWithBadOne[] = {
    0, // negative scenario
    CL_MEM_OBJECT_BUFFER,
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_SUITE_P(
    ImageArraySizeTest_Create,
    CreateImageOffset,
    testing::ValuesIn(allImageTypesWithBadOne));
