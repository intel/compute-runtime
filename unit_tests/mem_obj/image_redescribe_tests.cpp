/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"
#include "igfxfmid.h"

extern GFXCORE_FAMILY renderCoreFamily;

using namespace OCLRT;

class ImageRedescribeTest : public testing::TestWithParam<std::tuple<size_t, uint32_t>> {
  protected:
    void SetUp() override {

        cl_image_format imageFormat;
        cl_image_desc imageDesc;

        std::tie(indexImageFormat, ImageType) = this->GetParam();

        auto &surfaceFormatInfo = readWriteSurfaceFormats[indexImageFormat];
        imageFormat = surfaceFormatInfo.OCLImageFormat;

        auto imageHeight = ImageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 0 : 32;
        auto imageArrays = ImageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || ImageType == CL_MEM_OBJECT_IMAGE2D_ARRAY ? 7 : 1;

        imageDesc.image_type = ImageType;
        imageDesc.image_width = 32;
        imageDesc.image_height = imageHeight;
        imageDesc.image_depth = 1;
        imageDesc.image_array_size = imageArrays;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;

        retVal = CL_INVALID_VALUE;
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        image.reset(Image::create(
            &context,
            flags,
            surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));

        ASSERT_NE(nullptr, image);
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    std::unique_ptr<Image> image;
    size_t indexImageFormat = 0;
    uint32_t ImageType;
};

TEST_P(ImageRedescribeTest, givenImageWhenItIsRedescribedThenItContainsProperFormatFlagsAddressAndSameElementSizeInBytes) {
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);
    ASSERT_NE(image, imageNew);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), imageNew->getFlags() & CL_MEM_USE_HOST_PTR);
    EXPECT_EQ(image->getCpuAddress(), imageNew->getCpuAddress());
    EXPECT_NE(static_cast<cl_channel_type>(CL_FLOAT), imageNew->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type);
    EXPECT_NE(static_cast<cl_channel_type>(CL_HALF_FLOAT), imageNew->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type);
    EXPECT_EQ(imageNew->getSurfaceFormatInfo().NumChannels * imageNew->getSurfaceFormatInfo().PerChannelSizeInBytes,
              imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes);
    EXPECT_EQ(image->getSurfaceFormatInfo().ImageElementSizeInBytes,
              imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes);
}

TEST_P(ImageRedescribeTest, givenImageWhenItIsRedescribedThenNewImageFormatHasNumberOfChannelsDependingOnBytesPerPixel) {
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);

    size_t bytesPerPixel = image->getSurfaceFormatInfo().NumChannels * image->getSurfaceFormatInfo().PerChannelSizeInBytes;
    size_t channelsExpected = 0;
    switch (bytesPerPixel) {
    case 1:
    case 2:
    case 4:
        channelsExpected = 1;
        break;
    case 8:
        channelsExpected = 2;
        break;
    case 16:
        channelsExpected = 4;
        break;
    }
    EXPECT_EQ(channelsExpected, imageNew->getSurfaceFormatInfo().NumChannels);
}

TEST_P(ImageRedescribeTest, givenImageWhenItIsRedescribedThenNewImageDimensionsAreMatchingTheRedescribedImage) {
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);

    auto bytesWide = image->getSurfaceFormatInfo().ImageElementSizeInBytes * image->getImageDesc().image_width;
    auto bytesWideNew = imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes * imageNew->getImageDesc().image_width;

    EXPECT_EQ(bytesWide, bytesWideNew);
    EXPECT_EQ(imageNew->getImageDesc().image_height, image->getImageDesc().image_height);
    EXPECT_EQ(imageNew->getImageDesc().image_array_size, image->getImageDesc().image_array_size);
    EXPECT_EQ(imageNew->getImageDesc().image_depth, image->getImageDesc().image_depth);
    EXPECT_EQ(imageNew->getImageDesc().image_type, image->getImageDesc().image_type);
    EXPECT_EQ(imageNew->getQPitch(), image->getQPitch());
    EXPECT_EQ(imageNew->getImageDesc().image_width, image->getImageDesc().image_width);
}

TEST_P(ImageRedescribeTest, givenImageWhenItIsRedescribedThenCubeFaceIndexIsProperlySet) {
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);
    ASSERT_EQ(imageNew->getCubeFaceIndex(), __GMM_NO_CUBE_MAP);

    for (uint32_t n = __GMM_CUBE_FACE_POS_X; n < __GMM_MAX_CUBE_FACE; n++) {
        image->setCubeFaceIndex(n);
        imageNew.reset(image->redescribe());
        ASSERT_NE(nullptr, imageNew);
        ASSERT_EQ(imageNew->getCubeFaceIndex(), n);
        imageNew.reset(image->redescribeFillImage());
        ASSERT_NE(nullptr, imageNew);
        ASSERT_EQ(imageNew->getCubeFaceIndex(), n);
    }
}

TEST_P(ImageRedescribeTest, givenImageWithMaxSizesWhenItIsRedescribedThenNewImageDoesNotExceedMaxSizes) {
    cl_image_format imageFormat;
    cl_image_desc imageDesc;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    auto memoryManager = (OsAgnosticMemoryManager *)context.getMemoryManager();
    memoryManager->turnOnFakingBigAllocations();

    auto &surfaceFormatInfo = readWriteSurfaceFormats[indexImageFormat];
    imageFormat = surfaceFormatInfo.OCLImageFormat;

    auto imageWidth = 1;
    auto imageHeight = 1;
    auto imageArrays = ImageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || ImageType == CL_MEM_OBJECT_IMAGE2D_ARRAY ? 7 : 1;

    size_t maxImageWidth = 0;
    size_t maxImageHeight = 0;
    switch (ImageType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        imageWidth = 16384;
        maxImageWidth = static_cast<size_t>(caps.maxMemAllocSize);
        maxImageHeight = 1;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        imageHeight = 16384;
        maxImageWidth = caps.image2DMaxWidth;
        maxImageHeight = caps.image2DMaxHeight;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        imageHeight = 16384;
        maxImageWidth = caps.image3DMaxWidth;
        maxImageHeight = caps.image3DMaxHeight;
        break;
    }

    imageDesc.image_type = ImageType;
    imageDesc.image_width = imageWidth;
    imageDesc.image_height = imageHeight;
    imageDesc.image_depth = 1;
    imageDesc.image_array_size = imageArrays;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto bigImage = std::unique_ptr<Image>(Image::create(&context,
                                                         flags,
                                                         surfaceFormat,
                                                         &imageDesc,
                                                         nullptr,
                                                         retVal));

    std::unique_ptr<Image> imageNew(bigImage->redescribe());

    ASSERT_NE(nullptr, imageNew);

    EXPECT_GE(maxImageWidth,
              imageNew->getImageDesc().image_width);
    EXPECT_GE(maxImageHeight,
              imageNew->getImageDesc().image_height);
}

static uint32_t ImageType[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

decltype(numReadWriteSurfaceFormats) readWriteSurfaceFormatsStart = 0u;
INSTANTIATE_TEST_CASE_P(
    Redescribe,
    ImageRedescribeTest,
    testing::Combine(
        ::testing::Range(readWriteSurfaceFormatsStart, numReadWriteSurfaceFormats),
        ::testing::ValuesIn(ImageType)));
