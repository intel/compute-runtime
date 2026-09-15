/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(ImageTypeConversionTests, givenSupportedImageTypesWhenConvertingThenMatchingL0TypeIsReturned) {
    EXPECT_EQ(ZE_IMAGE_TYPE_1D, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE1D));
    EXPECT_EQ(ZE_IMAGE_TYPE_BUFFER, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE1D_BUFFER));
    EXPECT_EQ(ZE_IMAGE_TYPE_1DARRAY, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE1D_ARRAY));
    EXPECT_EQ(ZE_IMAGE_TYPE_2D, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE2D));
    EXPECT_EQ(ZE_IMAGE_TYPE_2DARRAY, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE2D_ARRAY));
    EXPECT_EQ(ZE_IMAGE_TYPE_3D, Image::clToL0ImageType(CL_MEM_OBJECT_IMAGE3D));
}

TEST(ImageTypeConversionTests, givenNonImageTypesWhenConvertingThenForceUint32IsReturned) {
    EXPECT_EQ(ZE_IMAGE_TYPE_FORCE_UINT32, Image::clToL0ImageType(CL_MEM_OBJECT_BUFFER));
    EXPECT_EQ(ZE_IMAGE_TYPE_FORCE_UINT32, Image::clToL0ImageType(CL_MEM_OBJECT_PIPE));
    EXPECT_EQ(ZE_IMAGE_TYPE_FORCE_UINT32, Image::clToL0ImageType(0));
}

TEST(ImageIsSrgbTests, givenSrgbChannelOrdersWhenCheckingThenReturnsTrue) {
    EXPECT_TRUE(Image::isSRGB(CL_sRGB));
    EXPECT_TRUE(Image::isSRGB(CL_sRGBA));
    EXPECT_TRUE(Image::isSRGB(CL_sBGRA));
}

TEST(ImageIsSrgbTests, givenNonSrgbChannelOrdersWhenCheckingThenReturnsFalse) {
    EXPECT_FALSE(Image::isSRGB(CL_RGB));
    EXPECT_FALSE(Image::isSRGB(CL_RGBA));
    EXPECT_FALSE(Image::isSRGB(CL_BGRA));
    EXPECT_FALSE(Image::isSRGB(CL_R));
    EXPECT_FALSE(Image::isSRGB(CL_sRGBx));
}

TEST(ImageFormatSwizzleTests, givenSingleChannelOrdersWhenConvertingThenSwizzlesMatch) {
    ze_image_format_t l0Format{};

    Image::clToL0ImageFormat(l0Format, CL_R, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_1, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8, l0Format.layout);

    Image::clToL0ImageFormat(l0Format, CL_A, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8, l0Format.layout);
}

TEST(ImageFormatSwizzleTests, givenTwoChannelOrdersWhenConvertingThenSwizzlesMatch) {
    ze_image_format_t l0Format{};

    Image::clToL0ImageFormat(l0Format, CL_RG, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_1, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8, l0Format.layout);

    Image::clToL0ImageFormat(l0Format, CL_RA, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8, l0Format.layout);
}

TEST(ImageFormatSwizzleTests, givenThreeChannelOrdersWhenConvertingThenSwizzlesMatch) {
    for (cl_channel_order channelOrder : {CL_RGB, CL_sRGB}) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_UNORM_INT8);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.y);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_B, l0Format.z);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_1, l0Format.w);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8_8, l0Format.layout);
    }
}

TEST(ImageFormatSwizzleTests, givenFourChannelOrdersWhenConvertingThenSwizzlesMatch) {
    ze_image_format_t l0Format{};

    for (cl_channel_order channelOrder : {CL_RGBA, CL_sRGBA}) {
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_UNORM_INT8);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.y);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_B, l0Format.z);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_A, l0Format.w);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, l0Format.layout);
    }

    for (cl_channel_order channelOrder : {CL_BGRA, CL_sBGRA}) {
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_UNORM_INT8);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_B, l0Format.x);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.y);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.z);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_A, l0Format.w);
    }

    Image::clToL0ImageFormat(l0Format, CL_ARGB, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_A, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_B, l0Format.w);

    Image::clToL0ImageFormat(l0Format, CL_ABGR, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_A, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_B, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_G, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.w);
}

TEST(ImageFormatSwizzleTests, givenLuminanceAndIntensityOrdersWhenConvertingThenChannelIsReplicated) {
    ze_image_format_t l0Format{};

    Image::clToL0ImageFormat(l0Format, CL_LUMINANCE, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_1, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8, l0Format.layout);

    Image::clToL0ImageFormat(l0Format, CL_INTENSITY, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_R, l0Format.w);
}

TEST(ImageFormatSwizzleTests, givenDepthOrdersWhenConvertingThenDepthSwizzleIsUsed) {
    for (cl_channel_order channelOrder : {CL_DEPTH, CL_DEPTH_STENCIL}) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_FLOAT);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_D, l0Format.x);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.y);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_0, l0Format.z);
        EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_1, l0Format.w);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_32, l0Format.layout);
    }
}

TEST(ImageFormatSwizzleTests, givenUnknownChannelOrderWhenConvertingThenSwizzlesAreForceUint32) {
    ze_image_format_t l0Format{};
    Image::clToL0ImageFormat(l0Format, 0xDEAD, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32, l0Format.x);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32, l0Format.y);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32, l0Format.z);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_FORCE_UINT32, l0Format.w);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8, l0Format.layout);
}

TEST(ImageFormatLayoutTests, givenNV12ChannelOrderWhenConvertingThenLayoutIsMappedAndSwizzlesUntouched) {
    ze_image_format_t l0Format{};
    l0Format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    Image::clToL0ImageFormat(l0Format, CL_NV12_INTEL, CL_UNORM_INT8);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_NV12, l0Format.layout);
    EXPECT_EQ(ZE_IMAGE_FORMAT_SWIZZLE_A, l0Format.x);
}

TEST(ImageFormatLayoutTests, givenEightBitChannelTypesWhenConvertingThenLayoutAndTypeMatch) {
    const std::pair<cl_channel_type, ze_image_format_type_t> eightBitTypes[] = {
        {CL_UNSIGNED_INT8, ZE_IMAGE_FORMAT_TYPE_UINT},
        {CL_SIGNED_INT8, ZE_IMAGE_FORMAT_TYPE_SINT},
        {CL_UNORM_INT8, ZE_IMAGE_FORMAT_TYPE_UNORM},
        {CL_SNORM_INT8, ZE_IMAGE_FORMAT_TYPE_SNORM}};

    for (const auto &[channelType, expectedType] : eightBitTypes) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, CL_RGBA, channelType);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, l0Format.layout);
        EXPECT_EQ(expectedType, l0Format.type);
    }
}

TEST(ImageFormatLayoutTests, givenSixteenBitChannelTypesWhenConvertingThenLayoutAndTypeMatch) {
    const std::pair<cl_channel_type, ze_image_format_type_t> sixteenBitTypes[] = {
        {CL_UNSIGNED_INT16, ZE_IMAGE_FORMAT_TYPE_UINT},
        {CL_SIGNED_INT16, ZE_IMAGE_FORMAT_TYPE_SINT},
        {CL_UNORM_INT16, ZE_IMAGE_FORMAT_TYPE_UNORM},
        {CL_SNORM_INT16, ZE_IMAGE_FORMAT_TYPE_SNORM},
        {CL_HALF_FLOAT, ZE_IMAGE_FORMAT_TYPE_FLOAT}};

    for (const auto &[channelType, expectedType] : sixteenBitTypes) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, CL_RG, channelType);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_16_16, l0Format.layout);
        EXPECT_EQ(expectedType, l0Format.type);
    }
}

TEST(ImageFormatLayoutTests, givenThirtyTwoBitChannelTypesWhenConvertingThenLayoutAndTypeMatch) {
    const std::pair<cl_channel_type, ze_image_format_type_t> thirtyTwoBitTypes[] = {
        {CL_UNSIGNED_INT32, ZE_IMAGE_FORMAT_TYPE_UINT},
        {CL_SIGNED_INT32, ZE_IMAGE_FORMAT_TYPE_SINT},
        {CL_FLOAT, ZE_IMAGE_FORMAT_TYPE_FLOAT}};

    for (const auto &[channelType, expectedType] : thirtyTwoBitTypes) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, CL_R, channelType);
        EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_32, l0Format.layout);
        EXPECT_EQ(expectedType, l0Format.type);
    }
}

TEST(ImageFormatLayoutTests, givenChannelCountWhenConvertingThenLayoutWidensAccordingly) {
    const std::pair<cl_channel_order, ze_image_format_layout_t> orders[] = {
        {CL_R, ZE_IMAGE_FORMAT_LAYOUT_8},
        {CL_RG, ZE_IMAGE_FORMAT_LAYOUT_8_8},
        {CL_RGB, ZE_IMAGE_FORMAT_LAYOUT_8_8_8},
        {CL_RGBA, ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8}};

    for (const auto &[channelOrder, expectedLayout] : orders) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, channelOrder, CL_UNORM_INT8);
        EXPECT_EQ(expectedLayout, l0Format.layout);
    }
}

TEST(ImageFormatLayoutTests, givenPackedChannelTypesWhenConvertingThenPackedLayoutIsUsed) {
    const std::pair<cl_channel_type, ze_image_format_layout_t> packedTypes[] = {
        {CL_UNORM_INT_101010_2, ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2},
        {CL_UNORM_INT24, ZE_IMAGE_FORMAT_LAYOUT_32},
        {CL_UNORM_SHORT_565, ZE_IMAGE_FORMAT_LAYOUT_5_6_5},
        {CL_UNORM_SHORT_555, ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1}};

    for (const auto &[channelType, expectedLayout] : packedTypes) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, CL_RGBA, channelType);
        EXPECT_EQ(expectedLayout, l0Format.layout);
        EXPECT_EQ(ZE_IMAGE_FORMAT_TYPE_UNORM, l0Format.type);
    }
}

TEST(ImageFormatLayoutTests, givenMediaChannelTypesWhenConvertingThenMediaLayoutIsUsed) {
    const std::pair<cl_channel_type, ze_image_format_layout_t> mediaTypes[] = {
        {CL_NV12_INTEL, ZE_IMAGE_FORMAT_LAYOUT_NV12},
        {CL_YUYV_INTEL, ZE_IMAGE_FORMAT_LAYOUT_YUYV},
        {CL_VYUY_INTEL, ZE_IMAGE_FORMAT_LAYOUT_VYUY},
        {CL_YVYU_INTEL, ZE_IMAGE_FORMAT_LAYOUT_YVYU},
        {CL_UYVY_INTEL, ZE_IMAGE_FORMAT_LAYOUT_UYVY}};

    for (const auto &[channelType, expectedLayout] : mediaTypes) {
        ze_image_format_t l0Format{};
        Image::clToL0ImageFormat(l0Format, CL_RGBA, channelType);
        EXPECT_EQ(expectedLayout, l0Format.layout);
        EXPECT_EQ(ZE_IMAGE_FORMAT_TYPE_UNORM, l0Format.type);
    }
}

TEST(ImageFormatLayoutTests, givenUnknownChannelTypeWhenConvertingThenLayoutIsForceUint32) {
    ze_image_format_t l0Format{};
    Image::clToL0ImageFormat(l0Format, CL_RGBA, 0xDEAD);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_FORCE_UINT32, l0Format.layout);
}

TEST(GetSurfaceFormatFromTableTests, givenSupportedReadWriteFormatWhenLookingUpThenEntryIsFound) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &format);
    ASSERT_NE(nullptr, surfaceFormat);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RGBA), surfaceFormat->oclImageFormat.image_channel_order);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), surfaceFormat->oclImageFormat.image_channel_data_type);
    EXPECT_EQ(4u, surfaceFormat->surfaceFormat.imageElementSizeInBytes);
}

TEST(GetSurfaceFormatFromTableTests, givenReadOnlyOnlyFormatWhenLookingUpWithReadWriteThenEntryIsNotFound) {
    cl_image_format format{CL_sRGBA, CL_UNORM_INT8};
    EXPECT_NE(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &format));
    EXPECT_EQ(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &format));
    EXPECT_EQ(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_WRITE_ONLY, &format));
}

TEST(GetSurfaceFormatFromTableTests, givenUnsupportedFormatWhenLookingUpThenNullIsReturned) {
    cl_image_format format{CL_RGB, CL_UNORM_SHORT_565};
    EXPECT_EQ(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &format));
}

TEST(GetSurfaceFormatFromTableTests, givenPlanarYuvFormatWhenLookingUpThenPlanarEntryIsFound) {
    cl_image_format format{CL_NV12_INTEL, CL_UNORM_INT8};
    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &format);
    ASSERT_NE(nullptr, surfaceFormat);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_NV12_INTEL), surfaceFormat->oclImageFormat.image_channel_order);
}

TEST(GetSurfaceFormatFromTableTests, givenPackedYuvFormatWhenLookingUpThenPackedEntryIsFound) {
    cl_image_format format{CL_UYVY_INTEL, CL_UNORM_INT8};
    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &format);
    ASSERT_NE(nullptr, surfaceFormat);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_UYVY_INTEL), surfaceFormat->oclImageFormat.image_channel_order);
}

TEST(GetSurfaceFormatFromTableTests, givenDepthFormatWhenLookingUpThenDepthEntryIsFound) {
    cl_image_format format{CL_DEPTH, CL_FLOAT};
    EXPECT_NE(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &format));
    EXPECT_NE(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &format));

    cl_image_format depthStencil{CL_DEPTH_STENCIL, CL_UNORM_INT24};
    EXPECT_NE(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &depthStencil));
    EXPECT_EQ(nullptr, Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &depthStencil));
}

TEST(GetRowPitchForImageFromBufferTests, givenNoParentBufferWhenGettingRowPitchThenDescriptorPitchIsReturned) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    imageDesc.mem_object = nullptr;
    imageDesc.image_width = 16;
    imageDesc.image_row_pitch = 0;

    EXPECT_EQ(0u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &format, &imageDesc));
}

TEST(GetRowPitchForImageFromBufferTests, givenExplicitRowPitchWhenGettingRowPitchThenDescriptorPitchIsReturned) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    imageDesc.mem_object = reinterpret_cast<cl_mem>(0x1234);
    imageDesc.image_width = 16;
    imageDesc.image_row_pitch = 256;

    EXPECT_EQ(256u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &format, &imageDesc));
}

TEST(GetRowPitchForImageFromBufferTests, givenParentBufferAndZeroPitchWhenGettingRowPitchThenPitchIsDerivedFromWidth) {
    cl_image_format format{CL_RGBA, CL_UNORM_INT8};
    cl_image_desc imageDesc{};
    imageDesc.mem_object = reinterpret_cast<cl_mem>(0x1234);
    imageDesc.image_width = 16;
    imageDesc.image_row_pitch = 0;

    EXPECT_EQ(16u * 4u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &format, &imageDesc));
}

TEST(GetRowPitchForImageFromBufferTests, givenParentBufferAndUnsupportedFormatWhenGettingRowPitchThenDescriptorPitchIsReturned) {
    cl_image_format format{CL_RGB, CL_UNORM_SHORT_565};
    cl_image_desc imageDesc{};
    imageDesc.mem_object = reinterpret_cast<cl_mem>(0x1234);
    imageDesc.image_width = 16;
    imageDesc.image_row_pitch = 0;

    EXPECT_EQ(0u, Image::getRowPitchForImageFromBuffer(CL_MEM_READ_WRITE, &format, &imageDesc));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
