/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/core/test/aub_tests/image/image_aub_fixture.h"

#include <cstring>
#include <tuple>
#include <vector>

namespace L0 {
namespace ult {

struct ImageAubFillChannelType {
    cl_channel_type type;
    uint32_t bytesPerChannel;
    const void *expectedValues;
};

inline constexpr uint32_t imageAubFillValues[4] = {0x3f800000, 0x00000000, 0x3f555555, 0x3f2aaaaa};

inline constexpr uint16_t imageAubFillExpectedHalfFloat[4] = {0x3c00, 0x0000, 0x3aaa, 0x3955};
inline constexpr uint16_t imageAubFillExpectedUnorm16[4] = {0xffff, 0x0000, 0xd554, 0xaaa9};
inline constexpr uint8_t imageAubFillExpectedUnorm8[4] = {0xff, 0x00, 0xd4, 0xa9};
inline constexpr uint8_t imageAubFillExpectedUnorm8Srgba[4] = {0xff, 0x00, 0xeb, 0xa9};
inline constexpr uint8_t imageAubFillExpectedUnorm8Sbgra[4] = {0xeb, 0x00, 0xff, 0xa9};
inline constexpr uint16_t imageAubFillExpectedSnorm16[4] = {0x7fff, 0x0000, 0x6AA9, 0x5554};
inline constexpr uint8_t imageAubFillExpectedSnorm8[4] = {0x7f, 0x00, 0x69, 0x54};
inline constexpr uint16_t imageAubFillExpectedInt16[4] = {0x0000, 0x0000, 0x5555, 0xaaaa};
inline constexpr uint8_t imageAubFillExpectedInt8[4] = {0x00, 0x00, 0x55, 0xaa};

inline constexpr ImageAubFillChannelType imageAubFillChannelTypes[] = {
    {CL_SNORM_INT8, 1u, imageAubFillExpectedSnorm8},
    {CL_SNORM_INT16, 2u, imageAubFillExpectedSnorm16},
    {CL_UNORM_INT8, 1u, imageAubFillExpectedUnorm8},
    {CL_UNORM_INT16, 2u, imageAubFillExpectedUnorm16},
    {CL_SIGNED_INT8, 1u, imageAubFillExpectedInt8},
    {CL_SIGNED_INT16, 2u, imageAubFillExpectedInt16},
    {CL_SIGNED_INT32, 4u, imageAubFillValues},
    {CL_UNSIGNED_INT8, 1u, imageAubFillExpectedInt8},
    {CL_UNSIGNED_INT16, 2u, imageAubFillExpectedInt16},
    {CL_UNSIGNED_INT32, 4u, imageAubFillValues},
    {CL_HALF_FLOAT, 2u, imageAubFillExpectedHalfFloat},
    {CL_FLOAT, 4u, imageAubFillValues}};

inline constexpr cl_channel_order imageAubFillChannelOrders[] = {CL_R, CL_RG, CL_RGBA, CL_sRGBA, CL_sBGRA};

using ImageAubFillParams = std::tuple<ImageAubFillChannelType, cl_channel_order, ImageAubRegionParams>;

struct AUBImageFillL0 : public ImageAubTestL0<::testing::TestWithParam<ImageAubFillParams>> {
    void SetUp() override {
        const auto channelType = std::get<0>(GetParam()).type;
        const auto channelOrder = std::get<1>(GetParam());
        if (channelType != CL_UNORM_INT8 && NEO::LEO::Image::isSRGB(channelOrder)) {
            GTEST_SKIP();
        }
        ImageAubFixtureL0::setUp(false, std::get<2>(GetParam()).imageType);
    }

    static uint32_t getNumChannels(cl_channel_order channelOrder) {
        switch (channelOrder) {
        case CL_R:
            return 1u;
        case CL_RG:
            return 2u;
        default:
            return 4u;
        }
    }
};

HWTEST2_P(AUBImageFillL0, givenImageRegionWhenFillingWithImageFillBuiltinThenOnlyRegionIsFilledWithConvertedColor, ImageSupport) {
    const auto &channelType = std::get<0>(GetParam());
    const auto channelOrder = std::get<1>(GetParam());
    const auto &regionParams = std::get<2>(GetParam());

    const uint32_t imageWidth = 5;
    const uint32_t imageHeight = regionParams.imageType != ZE_IMAGE_TYPE_1D ? 5 : 1;
    const uint32_t imageDepth = regionParams.imageType == ZE_IMAGE_TYPE_3D ? 5 : 1;
    const size_t elementSize = channelType.bytesPerChannel * getNumChannels(channelOrder);
    const size_t imageDataSize = elementSize * imageWidth * imageHeight * imageDepth;

    const cl_image_format clFormat = {channelOrder, channelType.type};
    ze_srgb_ext_desc_t srgbDesc = {ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, nullptr, NEO::LEO::Image::isSRGB(channelOrder)};
    ze_image_desc_t imageDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC, &srgbDesc};
    imageDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
    imageDesc.type = regionParams.imageType;
    NEO::LEO::Image::clToL0ImageFormat(imageDesc.format, channelOrder, channelType.type);
    imageDesc.width = imageWidth;
    imageDesc.height = imageHeight;
    imageDesc.depth = imageDepth;
    auto image = this->createImage(imageDesc);
    ASSERT_NE(nullptr, image);

    const uint8_t initialPattern = 0xAB;
    auto srcMemory = this->allocateHostBytes(imageDataSize);
    memset(srcMemory, initialPattern, imageDataSize);
    auto dstMemory = this->allocateHostBytes(imageDataSize);
    memset(dstMemory, 0, imageDataSize);

    const ze_image_region_t region = {regionParams.origin[0], regionParams.origin[1], regionParams.origin[2],
                                      std::max(imageWidth / 2, 1u), std::max(imageHeight / 2, 1u), std::max(imageDepth / 2, 1u)};

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, srcMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, NEO::LEO::Image::appendFillWithBuiltin(cmdList, this->device, image, clFormat, imageAubFillValues, region, 0u, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, dstMemory, image, nullptr, nullptr, 0, nullptr));
    this->synchronize();

    const void *expectedFillValue = channelType.expectedValues;
    if (channelOrder == CL_sRGBA) {
        expectedFillValue = imageAubFillExpectedUnorm8Srgba;
    } else if (channelOrder == CL_sBGRA) {
        expectedFillValue = imageAubFillExpectedUnorm8Sbgra;
    }

    std::vector<uint8_t> expectedMemory(imageDataSize, initialPattern);
    size_t offset = 0;
    for (uint32_t z = 0; z < imageDepth; z++) {
        for (uint32_t y = 0; y < imageHeight; y++) {
            for (uint32_t x = 0; x < imageWidth; x++) {
                if (isPixelInImageAubRegion(region, x, y, z)) {
                    memcpy(&expectedMemory[offset], expectedFillValue, elementSize);
                }
                offset += elementSize;
            }
        }
    }

    this->expectImmCmdListMemory(dstMemory, expectedMemory.data(), imageDataSize);
}

INSTANTIATE_TEST_SUITE_P(AUBImageFillL0,
                         AUBImageFillL0,
                         ::testing::Combine(::testing::ValuesIn(imageAubFillChannelTypes),
                                            ::testing::ValuesIn(imageAubFillChannelOrders),
                                            ::testing::ValuesIn(imageAubRegionParams)));

} // namespace ult
} // namespace L0
