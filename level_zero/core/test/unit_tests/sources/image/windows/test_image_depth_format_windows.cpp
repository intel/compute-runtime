/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

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
    zeDesc.width = 256;
    zeDesc.height = 256;

    auto imageHW = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = imageHW->initialize(device, &zeDesc);

    if (result == ZE_RESULT_SUCCESS) {
        auto clChannelOrder = getClChannelOrder(zeDesc.format);
        auto clChannelType = getClChannelDataType(zeDesc.format);

        EXPECT_EQ(clChannelOrder, static_cast<cl_channel_order>(CL_DEPTH));
        EXPECT_EQ(clChannelType, static_cast<cl_channel_type>(CL_FLOAT));
    }
}

} // namespace ult
} // namespace L0
