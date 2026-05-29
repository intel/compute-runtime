/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

extern GFXCORE_FAMILY renderCoreFamily;

using namespace NEO;

static constexpr uint32_t imageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

class ImageRedescribeTest : public ::testing::Test {
  protected:
    void createImage(size_t indexImageFormat, uint32_t imageType) {
        cl_image_format imageFormat;
        cl_image_desc imageDesc;

        ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
        auto &surfaceFormatInfo = readWriteSurfaceFormats[indexImageFormat];
        imageFormat = surfaceFormatInfo.oclImageFormat;

        auto imageHeight = imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 0 : 32;
        auto imageArrays = imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY ? 7 : 1;

        imageDesc.image_type = imageType;
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
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    std::unique_ptr<Image> image;
};

TEST_F(ImageRedescribeTest, givenImageWhenItIsRedescribedThenItContainsProperFormatFlagsAddressAndSameElementSizeInBytes) {
    for (size_t indexImageFormat = 0; indexImageFormat < SurfaceFormats::readWrite().size(); indexImageFormat++) {
        for (auto imageType : imageTypes) {
            createImage(indexImageFormat, imageType);
            ASSERT_NE(nullptr, image);

            std::unique_ptr<Image> imageNew(image->redescribe());
            ASSERT_NE(nullptr, imageNew);
            ASSERT_NE(image, imageNew);

            EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), imageNew->getFlags() & CL_MEM_USE_HOST_PTR);
            EXPECT_EQ(image->getCpuAddress(), imageNew->getCpuAddress());
            EXPECT_NE(static_cast<cl_channel_type>(CL_FLOAT), imageNew->getSurfaceFormatInfo().oclImageFormat.image_channel_data_type);
            EXPECT_NE(static_cast<cl_channel_type>(CL_HALF_FLOAT), imageNew->getSurfaceFormatInfo().oclImageFormat.image_channel_data_type);
            EXPECT_EQ(imageNew->getSurfaceFormatInfo().surfaceFormat.numChannels * imageNew->getSurfaceFormatInfo().surfaceFormat.perChannelSizeInBytes,
                      imageNew->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes);
            EXPECT_EQ(image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes,
                      imageNew->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes);
        }
    }
}

TEST_F(ImageRedescribeTest, givenImageWhenItIsRedescribedThenNewImageFormatHasNumberOfChannelsDependingOnBytesPerPixel) {
    for (size_t indexImageFormat = 0; indexImageFormat < SurfaceFormats::readWrite().size(); indexImageFormat++) {
        for (auto imageType : imageTypes) {
            createImage(indexImageFormat, imageType);
            ASSERT_NE(nullptr, image);

            std::unique_ptr<Image> imageNew(image->redescribe());
            ASSERT_NE(nullptr, imageNew);

            size_t bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.numChannels * image->getSurfaceFormatInfo().surfaceFormat.perChannelSizeInBytes;
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
            EXPECT_EQ(channelsExpected, imageNew->getSurfaceFormatInfo().surfaceFormat.numChannels);
        }
    }
}

TEST_F(ImageRedescribeTest, givenImageWhenItIsRedescribedThenNewImageDimensionsAreMatchingTheRedescribedImage) {
    for (size_t indexImageFormat = 0; indexImageFormat < SurfaceFormats::readWrite().size(); indexImageFormat++) {
        for (auto imageType : imageTypes) {
            createImage(indexImageFormat, imageType);
            ASSERT_NE(nullptr, image);

            std::unique_ptr<Image> imageNew(image->redescribe());
            ASSERT_NE(nullptr, imageNew);

            auto bytesWide = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * image->getImageDesc().image_width;
            auto bytesWideNew = imageNew->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * imageNew->getImageDesc().image_width;

            EXPECT_EQ(bytesWide, bytesWideNew);
            EXPECT_EQ(imageNew->getImageDesc().image_height, image->getImageDesc().image_height);
            EXPECT_EQ(imageNew->getImageDesc().image_array_size, image->getImageDesc().image_array_size);
            EXPECT_EQ(imageNew->getImageDesc().image_depth, image->getImageDesc().image_depth);
            EXPECT_EQ(imageNew->getImageDesc().image_type, image->getImageDesc().image_type);
            EXPECT_EQ(imageNew->getQPitch(), image->getQPitch());
            EXPECT_EQ(imageNew->getImageDesc().image_width, image->getImageDesc().image_width);
        }
    }
}

TEST_F(ImageRedescribeTest, givenImageWhenItIsRedescribedThenCubeFaceIndexIsProperlySet) {
    for (size_t indexImageFormat = 0; indexImageFormat < SurfaceFormats::readWrite().size(); indexImageFormat++) {
        for (auto imageType : imageTypes) {
            createImage(indexImageFormat, imageType);
            ASSERT_NE(nullptr, image);

            std::unique_ptr<Image> imageNew(image->redescribe());
            ASSERT_NE(nullptr, imageNew);
            ASSERT_EQ(imageNew->getCubeFaceIndex(), gmmNoCubeMap);

            for (GmmCubeFace n = __GMM_CUBE_FACE_POS_X; n < __GMM_MAX_CUBE_FACE; n++) {
                image->setCubeFaceIndex(n);
                imageNew.reset(image->redescribe());
                ASSERT_NE(nullptr, imageNew);
                ASSERT_EQ(imageNew->getCubeFaceIndex(), n);
                imageNew.reset(image->redescribeFillImage());
                ASSERT_NE(nullptr, imageNew);
                ASSERT_EQ(imageNew->getCubeFaceIndex(), n);
            }
        }
    }
}

TEST_F(ImageRedescribeTest, givenImageWithMaxSizesWhenItIsRedescribedThenNewImageDoesNotExceedMaxSizes) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    const auto &sharedCaps = device->getSharedDeviceInfo();

    auto memoryManager = (OsAgnosticMemoryManager *)context.getMemoryManager();
    memoryManager->turnOnFakingBigAllocations();

    ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
    for (size_t indexImageFormat = 0; indexImageFormat < readWriteSurfaceFormats.size(); indexImageFormat++) {
        for (auto imageType : imageTypes) {
            cl_image_format imageFormat;
            cl_image_desc imageDesc;

            auto &surfaceFormatInfo = readWriteSurfaceFormats[indexImageFormat];
            imageFormat = surfaceFormatInfo.oclImageFormat;

            auto imageWidth = 1;
            auto imageHeight = 1;
            auto imageArrays = imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY ? 7 : 1;

            size_t maxImageWidth = 0;
            size_t maxImageHeight = 0;
            switch (imageType) {
            case CL_MEM_OBJECT_IMAGE1D:
            case CL_MEM_OBJECT_IMAGE1D_ARRAY:
                imageWidth = 16384;
                maxImageWidth = static_cast<size_t>(sharedCaps.maxMemAllocSize);
                maxImageHeight = 1;
                break;
            case CL_MEM_OBJECT_IMAGE2D:
            case CL_MEM_OBJECT_IMAGE2D_ARRAY:
                imageHeight = 16384;
                maxImageWidth = sharedCaps.image2DMaxWidth;
                maxImageHeight = sharedCaps.image2DMaxHeight;
                break;
            case CL_MEM_OBJECT_IMAGE3D:
                imageHeight = 16384;
                maxImageWidth = caps.image3DMaxWidth;
                maxImageHeight = caps.image3DMaxHeight;
                break;
            }

            imageDesc.image_type = imageType;
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
            auto bigImage = std::unique_ptr<Image>(Image::create(
                &context,
                ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                flags,
                0,
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
    }
}

TEST(ImageRedescribeTestSimple, givenImageWhenItIsRedescribedThenCreateFunctionIsSameAsInOriginalImage) {
    MockContext context;
    std::unique_ptr<Image> image(ImageHelperUlt<Image1dDefaults>::create(&context));
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);
    EXPECT_EQ(image->createFunction, imageNew->createFunction);
}

TEST(ImageRedescribeTestSimple, givenImageWhenItIsRedescribedThenPlaneIsSameAsInOriginalImage) {
    MockContext context;
    std::unique_ptr<Image> image(ImageHelperUlt<Image1dDefaults>::create(&context));
    image->setPlane(ImagePlane::planeU);
    std::unique_ptr<Image> imageNew(image->redescribe());
    ASSERT_NE(nullptr, imageNew);
    EXPECT_EQ(image->getPlane(), imageNew->getPlane());
}
