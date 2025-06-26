/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

using ImageCreate = Test<DeviceFixture>;

HWTEST2_F(ImageCreate, WhenGettingImagePropertiesThenPropertiesSetCorrectly, IsXeHpgCore) {
    ze_image_properties_t properties;

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;
    desc.depth = 10;

    auto result = device->imageGetProperties(&desc, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto samplerFilterFlagsValid = (properties.samplerFilterFlags ==
                                    ZE_IMAGE_SAMPLER_FILTER_FLAG_POINT) ||
                                   (properties.samplerFilterFlags ==
                                    ZE_IMAGE_SAMPLER_FILTER_FLAG_LINEAR);
    EXPECT_TRUE(samplerFilterFlagsValid);
}

HWTEST2_F(ImageCreate, WhenDestroyingImageThenSuccessIsReturned, IsXeHpgCore) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    result = zeImageDestroy(image->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(ImageCreate, WhenCreatingImageThenSuccessIsReturned, IsXeHpgCore) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    image->destroy();
}

HWTEST2_F(ImageCreate, givenInvalidProductFamilyThenReturnNullPointer, IsXeHpgCore) {
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    L0::Image *imagePtr;

    auto result = Image::create(IGFX_UNKNOWN, device, &desc, &imagePtr);
    ASSERT_NE(ZE_RESULT_SUCCESS, result);

    auto image = whiteboxCast(imagePtr);
    ASSERT_EQ(nullptr, image);
}

HWTEST2_F(ImageCreate, WhenImagesIsCreatedThenParamsSetCorrectly, IsXeHpgCore) {
    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;
    desc.depth = 10;

    L0::Image *imagePtr;

    auto result = Image::create(productFamily, device, &desc, &imagePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto image = whiteboxCast(imagePtr);
    ASSERT_NE(nullptr, image);

    auto alloc = image->getAllocation();
    ASSERT_NE(nullptr, alloc);

    image->destroy();
}
} // namespace ult
} // namespace L0
