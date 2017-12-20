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
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"
#include "igfxfmid.h"

extern GFXCORE_FAMILY renderCoreFamily;

using namespace OCLRT;

typedef decltype(&Image::redescribe) RedescribeMethod;

class ImageRedescribeTest : public testing::TestWithParam<std::tuple<RedescribeMethod, size_t, uint32_t>> {
  public:
    ImageRedescribeTest()

    {
    }

  protected:
    void SetUp() override {

        cl_image_format imageFormat;
        cl_image_desc imageDesc;

        std::tie(redescribeMethod, indexImageFormat, ImageType) = this->GetParam();

        std::stringstream streamTestString;
        streamTestString << "Format: " << indexImageFormat;
        testString = streamTestString.str();

        auto &surfaceFormatInfo = readWriteSurfaceFormats[indexImageFormat];
        // clang-format off
        imageFormat = surfaceFormatInfo.OCLImageFormat;

        auto imageHeight = ImageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 0 : 32;
        auto imageArrays = ImageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || ImageType == CL_MEM_OBJECT_IMAGE2D_ARRAY ? 7 : 1;


        imageDesc.image_type        = ImageType;
        imageDesc.image_width       = 32;
        imageDesc.image_height      = imageHeight;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = imageArrays;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on

        retVal = CL_INVALID_VALUE;
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        image = Image::create(
            &context,
            flags,
            surfaceFormat,
            &imageDesc,
            nullptr,
            retVal);

        ASSERT_NE(nullptr, image);
    }

    void TearDown() override {
        delete image;
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    Image *image = nullptr;
    size_t indexImageFormat = 0;
    std::string testString;
    RedescribeMethod redescribeMethod;
    uint32_t ImageType;
};

TEST_P(ImageRedescribeTest, returnsImagePointer) {
    auto imageNew = image->redescribe();
    ASSERT_NE(image, imageNew) << testString;
    EXPECT_NE(nullptr, imageNew) << testString;
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageHasUseHostPtrFlags) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), imageNew->getFlags() & CL_MEM_USE_HOST_PTR) << testString;
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageHasSameAddress) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    EXPECT_EQ(image->getCpuAddress(), imageNew->getCpuAddress()) << testString;
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageHasNoFloatsOrHalfFloats) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    EXPECT_NE(static_cast<cl_channel_type>(CL_FLOAT), imageNew->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type) << testString;
    EXPECT_NE(static_cast<cl_channel_type>(CL_HALF_FLOAT), imageNew->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type) << testString;
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageFormatHasSameImageElementSizeInBytes) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    if (redescribeMethod == &Image::redescribe) {
        EXPECT_EQ(image->getSurfaceFormatInfo().ImageElementSizeInBytes,
                  imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes)
            << testString;
    } else {
        EXPECT_EQ(1u, imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes) << testString;
    }
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageFormatHasNumberOfChannelsDependingOnBytesPerPixel) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    if (redescribeMethod == &Image::redescribe) {
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
        EXPECT_EQ(channelsExpected,
                  imageNew->getSurfaceFormatInfo().NumChannels)
            << testString;
    } else {
        EXPECT_EQ(1u, imageNew->getSurfaceFormatInfo().NumChannels) << testString;
    }
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageFormatChannels_PerChannelSize_ElementSize_Jive) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    EXPECT_EQ(imageNew->getSurfaceFormatInfo().NumChannels * imageNew->getSurfaceFormatInfo().PerChannelSizeInBytes,
              imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes)
        << testString;
    delete imageNew;
}

TEST_P(ImageRedescribeTest, newImageDimensionsConsistent) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew) << testString;

    auto bytesWide = image->getSurfaceFormatInfo().ImageElementSizeInBytes * image->getImageDesc().image_width;
    auto bytesWideNew = imageNew->getSurfaceFormatInfo().ImageElementSizeInBytes * imageNew->getImageDesc().image_width;

    EXPECT_EQ(bytesWide, bytesWideNew) << testString;
    EXPECT_EQ(imageNew->getImageDesc().image_height, image->getImageDesc().image_height);
    EXPECT_EQ(imageNew->getImageDesc().image_array_size, image->getImageDesc().image_array_size);
    EXPECT_EQ(imageNew->getImageDesc().image_depth, image->getImageDesc().image_depth);
    EXPECT_EQ(imageNew->getImageDesc().image_type, image->getImageDesc().image_type);
    EXPECT_EQ(imageNew->getQPitch(), image->getQPitch());

    delete imageNew;
}

TEST_P(ImageRedescribeTest, VerifyCubeFaceIndices) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew);
    ASSERT_EQ(imageNew->getCubeFaceIndex(), __GMM_NO_CUBE_MAP);
    delete imageNew;

    for (uint32_t n = __GMM_CUBE_FACE_POS_X; n < __GMM_MAX_CUBE_FACE; n++) {
        image->setCubeFaceIndex(n);
        auto imageNew2 = image->redescribe();
        ASSERT_NE(nullptr, imageNew2);
        ASSERT_EQ(imageNew2->getCubeFaceIndex(), n);
        delete imageNew2;
    }

    for (uint32_t n = __GMM_CUBE_FACE_POS_X; n < __GMM_MAX_CUBE_FACE; n++) {
        image->setCubeFaceIndex(n);
        auto imageNew2 = image->redescribeFillImage();
        ASSERT_NE(nullptr, imageNew2);
        ASSERT_EQ(imageNew2->getCubeFaceIndex(), n);
        delete imageNew2;
    }
}

TEST_P(ImageRedescribeTest, newImageDoesNotExceedMaxSizes) {
    cl_image_format imageFormat;
    cl_image_desc imageDesc;

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
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

    auto imageNew = (bigImage.get()->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew);

    if (redescribeMethod == &Image::redescribe) {
        EXPECT_GE(maxImageWidth,
                  imageNew->getImageDesc().image_width);
        EXPECT_GE(maxImageHeight,
                  imageNew->getImageDesc().image_height);
    }
    delete imageNew;
}

static RedescribeMethod redescribeMethods[] = {
    &Image::redescribe};

static uint32_t ImageType[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

typedef decltype(numReadWriteSurfaceFormats) ReadWriteSurfaceFormatsCountType;
INSTANTIATE_TEST_CASE_P(
    Redescribe,
    ImageRedescribeTest,
    testing::Combine(
        ::testing::ValuesIn(redescribeMethods),
        ::testing::Range(static_cast<ReadWriteSurfaceFormatsCountType>(0u), numReadWriteSurfaceFormats),
        ::testing::ValuesIn(ImageType)));

typedef ImageRedescribeTest ImageRedescribeTestWidth;

TEST_P(ImageRedescribeTestWidth, newImageFormatHasSameWidth) {
    auto imageNew = (image->*redescribeMethod)();
    ASSERT_NE(nullptr, imageNew);

    EXPECT_EQ(image->getImageDesc().image_width,
              imageNew->getImageDesc().image_width);

    delete imageNew;
}

INSTANTIATE_TEST_CASE_P(
    Redescribe,
    ImageRedescribeTestWidth,
    testing::Combine(
        ::testing::Values(&Image::redescribe),
        ::testing::Range(static_cast<ReadWriteSurfaceFormatsCountType>(0u), numReadWriteSurfaceFormats),
        ::testing::ValuesIn(ImageType)));
