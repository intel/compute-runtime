/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "test.h"

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
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

TEST(ImageStaticFunctionDescriptorTest, givenZeImageDescWhenConvertDescriptorThenCorrectImageDescriptorReturned) {
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

using ImageSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
using ImageCreate = Test<DeviceFixture>;

HWTEST2_F(ImageCreate, givenValidImageDescriptionWhenImageCreateThenImageIsCreatedCorrectly, ImageSupport) {
    ze_image_desc_t zeDesc = {};
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                     ZE_IMAGE_FORMAT_TYPE_UINT,
                     ZE_IMAGE_FORMAT_SWIZZLE_R,
                     ZE_IMAGE_FORMAT_SWIZZLE_G,
                     ZE_IMAGE_FORMAT_SWIZZLE_B,
                     ZE_IMAGE_FORMAT_SWIZZLE_A};

    Image *image_ptr;
    auto result = Image::create(productFamily, device, &zeDesc, &image_ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    std::unique_ptr<L0::Image> image(image_ptr);

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

HWTEST2_F(ImageCreate, givenValidImageDescriptionWhenImageCreateWithUnsupportedImageThenNullPtrImageIsReturned, ImageSupport) {
    ze_image_desc_t zeDesc = {};
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
    zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

    zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_Y216};

    Image *image_ptr;
    auto result = Image::create(productFamily, device, &zeDesc, &image_ptr);

    ASSERT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT);
    ASSERT_EQ(image_ptr, nullptr);
}

class TestImageFormats : public DeviceFixture, public testing::TestWithParam<std::pair<ze_image_format_layout_t, ze_image_format_type_t>> {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

template <GFXCORE_FAMILY T>
struct WhiteBox<::L0::ImageCoreFamily<T>> : public ::L0::ImageCoreFamily<T> {
    using BaseClass = ::L0::ImageCoreFamily<T>;
    using BaseClass::redescribedSurfaceState;
    using BaseClass::surfaceState;
};

HWTEST2_F(ImageCreate, givenDifferentSwizzleFormatWhenImageInitializeThenCorrectSwizzleInRSSIsSet, ImageSupport) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    ze_image_desc_t desc = {};

    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);

    auto surfaceState = &imageHW->surfaceState;

    ASSERT_EQ(surfaceState->getShaderChannelSelectRed(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    ASSERT_EQ(surfaceState->getShaderChannelSelectGreen(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    ASSERT_EQ(surfaceState->getShaderChannelSelectBlue(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    ASSERT_EQ(surfaceState->getShaderChannelSelectAlpha(),
              RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
}

HWTEST2_P(TestImageFormats, givenValidLayoutAndTypeWhenCreateImageCoreFamilyThenValidImageIsCreated, ImageSupport) {
    auto params = GetParam();

    ze_image_desc_t zeDesc = {};
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 10u;
    zeDesc.height = 10u;
    zeDesc.width = 10u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;

    zeDesc.format = {};
    zeDesc.format.layout = params.first;
    zeDesc.format.type = params.second;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();

    imageHW->initialize(device, &zeDesc);

    EXPECT_EQ(imageHW->getAllocation()->getAllocationType(), NEO::GraphicsAllocation::AllocationType::IMAGE);
    auto RSS = imageHW->surfaceState;
    EXPECT_EQ(RSS.getSurfaceType(), FamilyType::RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(RSS.getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(RSS.getRenderTargetViewExtent(), 1u);

    auto hAlign = static_cast<typename FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(imageHW->getAllocation()->getDefaultGmm()->gmmResourceInfo->getHAlignSurfaceState());
    auto vAlign = static_cast<typename FamilyType::RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(imageHW->getAllocation()->getDefaultGmm()->gmmResourceInfo->getVAlignSurfaceState());

    EXPECT_EQ(RSS.getSurfaceHorizontalAlignment(), hAlign);
    EXPECT_EQ(RSS.getSurfaceVerticalAlignment(), vAlign);

    auto isMediaFormatLayout = imageHW->isMediaFormat(params.first);
    if (isMediaFormatLayout) {
        auto imgInfo = imageHW->getImageInfo();
        EXPECT_EQ(RSS.getShaderChannelSelectAlpha(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
        EXPECT_EQ(RSS.getYOffsetForUOrUvPlane(), imgInfo.yOffsetForUVPlane);
        EXPECT_EQ(RSS.getXOffsetForUOrUvPlane(), imgInfo.xOffset);
    } else {
        EXPECT_EQ(RSS.getYOffsetForUOrUvPlane(), 0u);
        EXPECT_EQ(RSS.getXOffsetForUOrUvPlane(), 0u);
    }

    EXPECT_EQ(RSS.getSurfaceMinLod(), 0u);
    EXPECT_EQ(RSS.getMipCountLod(), 0u);

    if (!isMediaFormatLayout) {
        EXPECT_EQ(RSS.getShaderChannelSelectRed(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_RED);
        EXPECT_EQ(RSS.getShaderChannelSelectGreen(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_GREEN);
        EXPECT_EQ(RSS.getShaderChannelSelectBlue(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_BLUE);
        EXPECT_EQ(RSS.getShaderChannelSelectAlpha(), FamilyType::RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_ALPHA);
    }

    EXPECT_EQ(RSS.getNumberOfMultisamples(), FamilyType::RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
}

std::pair<ze_image_format_layout_t, ze_image_format_type_t> validFormats[] = {
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UINT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_SINT},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_SNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_FLOAT},
    {ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_Y8, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_AYUV, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P010, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_Y410, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P012, ZE_IMAGE_FORMAT_TYPE_UNORM},
    {ZE_IMAGE_FORMAT_LAYOUT_P016, ZE_IMAGE_FORMAT_TYPE_UNORM}};

INSTANTIATE_TEST_CASE_P(
    validImageFormats,
    TestImageFormats,
    testing::ValuesIn(validFormats));

TEST(ImageFormatDescHelperTest, givenUnsupportedImageFormatLayoutAndTypeThenProperClEnumIsReturned) {
    auto invalid = static_cast<cl_channel_type>(CL_INVALID_VALUE);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_UNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_11_11_10, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_SINT}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_SNORM}), invalid);
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_FLOAT}), invalid);
}

TEST(ImageFormatDescHelperTest, givenSupportedImageFormatLayoutAndTypeThenProperClEnumIsReturned) {
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT8));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_SNORM}), static_cast<cl_channel_type>(CL_SNORM_INT16));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_HALF_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT}), static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT}), static_cast<cl_channel_type>(CL_SIGNED_INT32));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT}), static_cast<cl_channel_type>(CL_FLOAT));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_INT_101010_2));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_6_5, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_SHORT_565));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UNORM_SHORT_555));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_NV12_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YUYV, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_YUYV_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_VYUY, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_VYUY_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_YVYU, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_YVYU_INTEL));
    EXPECT_EQ(getClChannelDataType({ZE_IMAGE_FORMAT_LAYOUT_UYVY, ZE_IMAGE_FORMAT_TYPE_UNORM}), static_cast<cl_channel_type>(CL_UYVY_INTEL));
}

TEST(ImageFormatDescHelperTest, givenSwizzlesThenEqualityIsProperlyDetermined) {
    swizzles ref{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_FALSE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_0}));
    EXPECT_TRUE((ref == swizzles{ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A}));
}

TEST(ImageFormatDescHelperTest, givenSupportedSwizzlesThenProperClEnumIsReturned) {
    ze_image_format_t format{};

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_R));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_A));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RG));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RA));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RGB));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_RGBA));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_B;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_ARGB));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_ABGR));

    format.x = ZE_IMAGE_FORMAT_SWIZZLE_B;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    EXPECT_EQ(getClChannelOrder(format), static_cast<cl_channel_order>(CL_BGRA));
}

} // namespace ult
} // namespace L0
