/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/mocks/mock_device.h"
#include "test.h"

#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

struct ImageStaticFunctionConvertTypeTest : public testing::TestWithParam<std::pair<ze_image_type_t, NEO::ImageType>> {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_P(ImageStaticFunctionConvertTypeTest, givenZeImageFormatTypeWhenConvertTypeThenCorrectImageTypeReturned) {
    auto params = GetParam();
    EXPECT_EQ(ImageImp::convertType(params.first), params.second);
}

std::pair<ze_image_type_t, NEO::ImageType> validTypes[] = {
    {ZE_IMAGE_TYPE_2D, NEO::ImageType::Image2D},
    {ZE_IMAGE_TYPE_3D, NEO::ImageType::Image3D},
    {ZE_IMAGE_TYPE_2DARRAY, NEO::ImageType::Image2DArray},
    {ZE_IMAGE_TYPE_1D, NEO::ImageType::Image1D},
    {ZE_IMAGE_TYPE_1DARRAY, NEO::ImageType::Image1DArray},
    {ZE_IMAGE_TYPE_BUFFER, NEO::ImageType::Image1DBuffer}};

INSTANTIATE_TEST_CASE_P(
    imageTypeFlags,
    ImageStaticFunctionConvertTypeTest,
    testing::ValuesIn(validTypes));

struct ImageStaticFunctionDescriptorFixture {
    void SetUp() {
    }

    void TearDown() {
    }
};

using ImageStaticFunctionDescriptorTest = Test<ImageStaticFunctionDescriptorFixture>;

TEST_F(ImageStaticFunctionDescriptorTest, givenZeImageDescWhenConvertDescriptorThenCorrectImageDescriptorReturned) {
    ze_image_desc_t zeDesc = {};
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;

    NEO::ImageDescriptor desc = ImageImp::convertDescriptor(zeDesc);
    EXPECT_EQ(desc.fromParent, false);
    EXPECT_EQ(desc.imageArraySize, zeDesc.arraylevels);
    EXPECT_EQ(desc.imageDepth, zeDesc.depth);
    EXPECT_EQ(desc.imageHeight, zeDesc.height);
    EXPECT_EQ(desc.imageRowPitch, 0u);
    EXPECT_EQ(desc.imageSlicePitch, 0u);
    EXPECT_EQ(desc.imageType, NEO::ImageType::Image2DArray);
    EXPECT_EQ(desc.imageWidth, zeDesc.width);
    EXPECT_EQ(desc.numMipLevels, zeDesc.miplevels);
    EXPECT_EQ(desc.numSamples, 0u);
}

struct DeviceFixture {
    void SetUp() {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        device = std::make_unique<Mock<L0::DeviceImp>>(neoDevice, neoDevice->getExecutionEnvironment());
    }

    void TearDown() {
    }

    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DeviceImp>> device = nullptr;
};

using ImageSupport = IsAtMostProduct<IGFX_TIGERLAKE_LP>;
using ImageCreate = Test<DeviceFixture>;

HWTEST2_F(ImageCreate, givenValidImageDescriptionWhenImageCreateThenImageIsCreatedCorrectly, ImageSupport) {
    ze_image_desc_t zeDesc = {};
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.version = ZE_IMAGE_DESC_VERSION_CURRENT;
    zeDesc.flags = ZE_IMAGE_FLAG_PROGRAM_READ;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    std::unique_ptr<L0::Image> image(Image::create(productFamily, device.get(), &zeDesc));

    ASSERT_NE(image, nullptr);

    auto imageInfo = image->getImageInfo();

    EXPECT_EQ(imageInfo.imgDesc.fromParent, false);
    EXPECT_EQ(imageInfo.imgDesc.imageArraySize, zeDesc.arraylevels);
    EXPECT_EQ(imageInfo.imgDesc.imageDepth, zeDesc.depth);
    EXPECT_EQ(imageInfo.imgDesc.imageHeight, zeDesc.height);
    EXPECT_EQ(imageInfo.imgDesc.imageType, NEO::ImageType::Image2DArray);
    EXPECT_EQ(imageInfo.imgDesc.imageWidth, zeDesc.width);
    EXPECT_EQ(imageInfo.imgDesc.numMipLevels, zeDesc.miplevels);
    EXPECT_EQ(imageInfo.imgDesc.numSamples, 0u);
    EXPECT_EQ(imageInfo.baseMipLevel, 0u);
    EXPECT_EQ(imageInfo.linearStorage, false);
    EXPECT_EQ(imageInfo.mipCount, 0u);
    EXPECT_EQ(imageInfo.plane, GMM_NO_PLANE);
    EXPECT_EQ(imageInfo.preferRenderCompression, false);
    EXPECT_EQ(imageInfo.useLocalMemory, false);
}

} // namespace ult
} // namespace L0
