/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

#include "CL/cl_gl.h"
#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace ult {

using ImageDepthFormatWindowsTest = Test<DeviceFixture>;

TEST_F(ImageDepthFormatWindowsTest, givenDepthSwizzleFormatWhenConvertingToClChannelOrderThenClDepthIsReturned) {
    ze_image_format_t format{};
    format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    auto clChannelOrder = getClChannelOrder(format);
    EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
}

TEST_F(ImageDepthFormatWindowsTest, givenDepthSwizzleFormatWhenConvertingToClChannelTypeThenClFloatIsReturned) {
    ze_image_format_t format{};
    format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    auto clChannelType = getClChannelDataType(format);
    EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_FLOAT));
}

TEST_F(ImageDepthFormatWindowsTest, givenDepth16UnormSwizzleFormatWhenConvertingToClChannelTypeThenClUnormInt16IsReturned) {
    ze_image_format_t format{};
    format.layout = ZE_IMAGE_FORMAT_LAYOUT_16;
    format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
    format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;

    auto clChannelOrder = getClChannelOrder(format);
    auto clChannelType = getClChannelDataType(format);

    EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
    EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_UNORM_INT16));
}

HWTEST_F(ImageDepthFormatWindowsTest, givenDepthFormatWhenCreatingImageDescThenCorrectFormatIsUsed) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    zeDesc.width = 16;
    zeDesc.height = 16;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = imageHW->initialize(device, &zeDesc);

    if (result == ZE_RESULT_SUCCESS) {
        auto clChannelOrder = getClChannelOrder(zeDesc.format);
        auto clChannelType = getClChannelDataType(zeDesc.format);

        EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
        EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_FLOAT));
    }
}

TEST_F(ImageDepthFormatWindowsTest, givenDepthStencilImageWhenOverridingChannelTypeThenUnormInt24IsReturned) {
    cl_channel_type channelType = CL_INVALID_VALUE;
    auto result = ImageImp::overrideChannelTypeForDepthInt24Image(channelType, true, nullptr);
    EXPECT_EQ(result, static_cast<cl_channel_type>(CL_UNORM_INT24));
}

TEST_F(ImageDepthFormatWindowsTest, givenDepthStencilImageWithFloatChannelTypeWhenOverridingThenFloatIsPreserved) {
    cl_channel_type channelType = CL_FLOAT;
    auto result = ImageImp::overrideChannelTypeForDepthInt24Image(channelType, true, nullptr);
    EXPECT_EQ(result, static_cast<cl_channel_type>(CL_FLOAT));
}

TEST_F(ImageDepthFormatWindowsTest, givenNonDepthStencilImageWhenOverridingChannelTypeThenOriginalValueIsReturned) {
    cl_channel_type channelType = CL_INVALID_VALUE;
    auto result = ImageImp::overrideChannelTypeForDepthInt24Image(channelType, false, nullptr);
    EXPECT_EQ(result, static_cast<cl_channel_type>(CL_INVALID_VALUE));
}

TEST_F(ImageDepthFormatWindowsTest, givenNonDepthStencilImageWithValidChannelTypeWhenOverridingThenOriginalValueIsReturned) {
    cl_channel_type channelType = CL_UNSIGNED_INT32;
    auto result = ImageImp::overrideChannelTypeForDepthInt24Image(channelType, false, nullptr);
    EXPECT_EQ(result, static_cast<cl_channel_type>(CL_UNSIGNED_INT32));
}

HWTEST_F(ImageDepthFormatWindowsTest, givenDepthStencilD24S8ImageWhenPopulatingImplicitArgsThenChannelTypeIsUnormInt24AndChannelOrderIsDepth) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_UNORM;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    zeDesc.width = 16;
    zeDesc.height = 16;

    ze_depth_stencil_format_ext_desc_t dsDesc{};
    dsDesc.format = ZE_DEPTH_STENCIL_FORMAT_D24_UNORM_S8_UINT;
    zeDesc.pNext = &dsDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = imageHW->initialize(device, &zeDesc);

    if (result == ZE_RESULT_SUCCESS) {
        EXPECT_TRUE(imageHW->isDepthStencil());

        auto clChannelOrder = getClChannelOrder(imageHW->getImageDesc().format);
        auto clChannelType = getClChannelDataType(imageHW->getImageDesc().format);
        clChannelType = ImageImp::overrideChannelTypeForDepthInt24Image(clChannelType, imageHW->isDepthStencil(), imageHW->getAllocation());

        EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
        EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_UNORM_INT24));
    }
}

HWTEST_F(ImageDepthFormatWindowsTest, givenDepthStencilD32FS8ImageWhenPopulatingImplicitArgsThenChannelTypeIsFloatAndChannelOrderIsDepth) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_FLOAT;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_D;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_0;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_1;
    zeDesc.width = 16;
    zeDesc.height = 16;

    ze_depth_stencil_format_ext_desc_t dsDesc{};
    dsDesc.format = ZE_DEPTH_STENCIL_FORMAT_D32_FLOAT_S8X24_UINT;
    zeDesc.pNext = &dsDesc;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = imageHW->initialize(device, &zeDesc);

    if (result == ZE_RESULT_SUCCESS) {
        EXPECT_TRUE(imageHW->isDepthStencil());

        auto clChannelOrder = getClChannelOrder(imageHW->getImageDesc().format);
        auto clChannelType = getClChannelDataType(imageHW->getImageDesc().format);
        clChannelType = ImageImp::overrideChannelTypeForDepthInt24Image(clChannelType, imageHW->isDepthStencil(), imageHW->getAllocation());

        EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
        EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_FLOAT));
    }
}

} // namespace ult
} // namespace L0
