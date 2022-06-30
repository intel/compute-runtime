/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct clGetImageInfoTests : public ApiFixture<>,
                             public ::testing::Test {

    void SetUp() override {
        ApiFixture::SetUp();

        imageFormat.image_channel_order = CL_RGBA;
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = 32;
        imageDesc.image_height = 32;
        imageDesc.image_depth = 1;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = nullptr;

        image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, image);
    }

    void TearDown() override {
        retVal = clReleaseMemObject(image);
        EXPECT_EQ(CL_SUCCESS, retVal);

        ApiFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_mem image;
};

TEST_F(clGetImageInfoTests, GivenBufferWhenGettingImageInfoThenInvalidMemObjectErrorIsReturned) {
    size_t paramRetSize = 0;
    auto buffer = clCreateBuffer(pContext, CL_MEM_READ_WRITE, 42, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetImageInfo(buffer, CL_IMAGE_ELEMENT_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
    clReleaseMemObject(buffer);
}

TEST_F(clGetImageInfoTests, GivenNullWhenGettingImageInfoThenInvalidMemObjectErrorIsReturned) {
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(nullptr, CL_IMAGE_ELEMENT_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clGetImageInfoTests, GivenInvalidParamNameWhenGettingImageInfoThenInvalidValueErrorIsReturned) {
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_MEM_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    ASSERT_EQ(0u, paramRetSize);
}

TEST_F(clGetImageInfoTests, GivenInvalidParametersWhenGettingImageInfoThenValueSizeRetIsNotUpdated) {
    size_t paramRetSize = 0x1234;

    retVal = clGetImageInfo(image, CL_MEM_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramRetSize);
}

TEST_F(clGetImageInfoTests, GivenClImageFormatWhenGettingImageInfoThenImageFormatIsReturned) {
    cl_image_format imgFmtRet;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_FORMAT, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_image_format), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_FORMAT, paramRetSize, &imgFmtRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageFormat.image_channel_data_type, imgFmtRet.image_channel_data_type);
    ASSERT_EQ(this->imageFormat.image_channel_order, imgFmtRet.image_channel_order);
}

TEST_F(clGetImageInfoTests, GivenClImageElementSizeWhenGettingImageInfoThenSizeOfImageElementIsReturned) {
    size_t elemSize = 4;
    size_t sizeRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, paramRetSize, &sizeRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(elemSize, sizeRet);
}

TEST_F(clGetImageInfoTests, GivenClImageRowPitchWhenGettingImageInfoThenSizeOfRowIsReturned) {
    size_t rowPitchRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_ROW_PITCH, paramRetSize, &rowPitchRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, rowPitchRet);
}

TEST_F(clGetImageInfoTests, GivenClImageSlicePitchAnd2dImageWhenGettingImageInfoThenZeroIsReturned) {
    size_t slicePitchRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_SLICE_PITCH, paramRetSize, &slicePitchRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(0u, slicePitchRet);
}

TEST_F(clGetImageInfoTests, GivenClImageWidthWhenGettingImageInfoThenWidthOfImageIsReturned) {
    size_t widthRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, paramRetSize, &widthRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageDesc.image_width, widthRet);
}

TEST_F(clGetImageInfoTests, GivenImageWithMipMapsWhenGettingImageInfoThenWidthAndHeightOfImageAreShifted) {
    auto initialWidth = this->imageDesc.image_width;
    auto initialHeight = this->imageDesc.image_height;

    auto pImage = castToObject<Image>(image);

    size_t returnValue = 0;
    size_t paramRetSize = sizeof(size_t);

    for (int mipLevel = 0; mipLevel < 10; mipLevel++) {
        pImage->setBaseMipLevel(mipLevel);
        auto expectedWidth = initialWidth >> mipLevel;
        expectedWidth = expectedWidth == 0 ? 1 : expectedWidth;
        auto expectedHeight = initialHeight >> mipLevel;
        expectedHeight = expectedHeight == 0 ? 1 : expectedHeight;

        retVal = clGetImageInfo(image, CL_IMAGE_WIDTH, paramRetSize, &returnValue, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedWidth, returnValue);
        retVal = clGetImageInfo(image, CL_IMAGE_HEIGHT, paramRetSize, &returnValue, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedHeight, returnValue);
    }
}

TEST_F(clGetImageInfoTests, GivenClImageHeightWhenGettingImageInfoThenHeightOfImageIsReturned) {
    size_t heightRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_HEIGHT, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_HEIGHT, paramRetSize, &heightRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageDesc.image_height, heightRet);
}

TEST_F(clGetImageInfoTests, Given3dImageWithMipMapsWhenGettingImageInfoThenWidthAndHeightOfImageAreShifted) {
    size_t widthRet;
    size_t expectedWidth;
    size_t heightRet;
    size_t expectedHeight;
    size_t depthRet;
    size_t expectedDepth;
    cl_image_format imageFormat2;
    cl_image_desc imageDesc2;
    cl_mem image2;

    imageFormat2.image_channel_order = CL_RGBA;
    imageFormat2.image_channel_data_type = CL_UNORM_INT8;

    imageDesc2.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc2.image_width = 8;
    imageDesc2.image_height = 8;
    imageDesc2.image_depth = 4;
    imageDesc2.image_array_size = 1;
    imageDesc2.image_row_pitch = 0;
    imageDesc2.image_slice_pitch = 0;
    imageDesc2.num_mip_levels = 5;
    imageDesc2.num_samples = 0;
    imageDesc2.mem_object = nullptr;

    image2 = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat2, &imageDesc2, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image2);

    auto pImgObj = castToObject<Image>(image2);
    for (cl_uint n = 0; n <= imageDesc2.num_mip_levels; n++) {
        pImgObj->setBaseMipLevel(n);

        retVal = clGetImageInfo(image2, CL_IMAGE_WIDTH, sizeof(widthRet), &widthRet, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        expectedWidth = imageDesc2.image_width >> n;
        expectedWidth = (expectedWidth == 0) ? 1 : expectedWidth;
        ASSERT_EQ(expectedWidth, widthRet);

        retVal = clGetImageInfo(image2, CL_IMAGE_HEIGHT, sizeof(heightRet), &heightRet, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        expectedHeight = imageDesc2.image_height >> n;
        expectedHeight = (expectedHeight == 0) ? 1 : expectedHeight;
        ASSERT_EQ(expectedHeight, heightRet);

        retVal = clGetImageInfo(image2, CL_IMAGE_DEPTH, sizeof(depthRet), &depthRet, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        expectedDepth = imageDesc2.image_depth >> n;
        expectedDepth = (expectedDepth == 0) ? 1 : expectedDepth;
        ASSERT_EQ(expectedDepth, depthRet);
    }

    retVal = clReleaseMemObject(image2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetImageInfoTests, Given1dImageWithMipMapsWhenGettingImageInfoThenWidthAndHeightOfImageAreShifted) {
    size_t widthRet;
    size_t expectedWidth;
    size_t heightRet;
    size_t depthRet;
    cl_image_format imageFormat2;
    cl_image_desc imageDesc2;
    cl_mem image2;

    imageFormat2.image_channel_order = CL_RGBA;
    imageFormat2.image_channel_data_type = CL_UNORM_INT8;

    imageDesc2.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc2.image_width = 8;
    imageDesc2.image_height = 1;
    imageDesc2.image_depth = 1;
    imageDesc2.image_array_size = 1;
    imageDesc2.image_row_pitch = 0;
    imageDesc2.image_slice_pitch = 0;
    imageDesc2.num_mip_levels = 5;
    imageDesc2.num_samples = 0;
    imageDesc2.mem_object = nullptr;

    image2 = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat2, &imageDesc2, nullptr, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image2);

    auto pImgObj = castToObject<Image>(image2);
    for (cl_uint n = 0; n <= imageDesc2.num_mip_levels; n++) {
        pImgObj->setBaseMipLevel(n);

        retVal = clGetImageInfo(image2, CL_IMAGE_WIDTH, sizeof(widthRet), &widthRet, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        expectedWidth = imageDesc2.image_width >> n;
        expectedWidth = (expectedWidth == 0) ? 1 : expectedWidth;
        ASSERT_EQ(expectedWidth, widthRet);

        retVal = clGetImageInfo(image2, CL_IMAGE_HEIGHT, sizeof(heightRet), &heightRet, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(0u, heightRet);

        retVal = clGetImageInfo(image2, CL_IMAGE_DEPTH, sizeof(depthRet), &depthRet, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(0u, depthRet);
    }

    retVal = clReleaseMemObject(image2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetImageInfoTests, GivenClImageDepthAnd2dImageWhenGettingImageInfoThenZeroIsReturned) {
    size_t depthRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_DEPTH, paramRetSize, &depthRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(0U, depthRet);
}

TEST_F(clGetImageInfoTests, GivenClImageArraySizeAndNonArrayImageWhenGettingImageInfoThenZeroIsReturned) {
    size_t arraySizeRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(size_t), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, paramRetSize, &arraySizeRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(0u, arraySizeRet);
}

TEST_F(clGetImageInfoTests, GivenClImageBufferWhenGettingImageInfoThenBufferIsReturned) {
    cl_mem bufferRet = nullptr;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_BUFFER, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_mem), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_BUFFER, paramRetSize, &bufferRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageDesc.buffer, bufferRet);
}

TEST_F(clGetImageInfoTests, GivenClImageNumMipLevelsWhenGettingImageInfoThenCorrectMipMapLevelIsReturned) {
    cl_uint numMipLevelRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_uint), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_NUM_MIP_LEVELS, paramRetSize, &numMipLevelRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageDesc.num_mip_levels, numMipLevelRet);
}

TEST_F(clGetImageInfoTests, GivenClImageNumSamplesWhenGettingImageInfoThenCorrectNumberOfSamplesIsReturned) {
    cl_uint numSamplesRet = 0;
    size_t paramRetSize = 0;

    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, 0, nullptr, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_uint), paramRetSize);

    retVal = clGetImageInfo(image, CL_IMAGE_NUM_SAMPLES, paramRetSize, &numSamplesRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(this->imageDesc.num_samples, numSamplesRet);
}

TEST_F(clGetImageInfoTests, givenMultisampleCountForMcsWhenAskingForRowPitchThenReturnNewValueIfGreaterThanOne) {
    McsSurfaceInfo mcsInfo = {1, 1, 0};
    imageDesc.num_samples = 16;
    size_t receivedRowPitch = 0;

    clReleaseMemObject(image);
    image = Image::validateAndCreateImage(pContext, nullptr, CL_MEM_READ_WRITE, 0, &imageFormat, &imageDesc, nullptr, retVal);

    auto imageObj = castToObject<Image>(image);
    auto formatInfo = imageObj->getSurfaceFormatInfo();

    size_t multisampleRowPitch = imageDesc.image_width * formatInfo.surfaceFormat.ImageElementSizeInBytes * imageDesc.num_samples;
    EXPECT_NE(multisampleRowPitch, imageObj->getHostPtrRowPitch());

    for (uint32_t multisampleCount = 0; multisampleCount <= 4; multisampleCount++) {
        mcsInfo.multisampleCount = multisampleCount;
        imageObj->setMcsSurfaceInfo(mcsInfo);

        clGetImageInfo(image, CL_IMAGE_ROW_PITCH, sizeof(size_t), &receivedRowPitch, nullptr);

        if (multisampleCount > 1) {
            EXPECT_EQ(multisampleRowPitch, receivedRowPitch);
        } else {
            EXPECT_EQ(imageObj->getHostPtrRowPitch(), receivedRowPitch);
        }
    }
}
} // namespace ULT
