/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/image/image_aub_fixture.h"

#include <tuple>

namespace L0 {
namespace ult {

struct ImageAubCopyRegionOffsets {
    uint32_t srcOffset;
    uint32_t dstOffset;
};

inline constexpr ImageAubCopyRegionOffsets imageAubCopyRegionOffsets[] = {{0u, 0u}, {1u, 2u}, {2u, 1u}};

struct AUBImageCopyRegionL0 : public ImageAubTestL0<::testing::TestWithParam<std::tuple<ImageAubCopyRegionOffsets, bool>>> {
    void SetUp() override {
        ImageAubFixtureL0::setUp(std::get<1>(GetParam()), ZE_IMAGE_TYPE_2D);
    }
};

HWTEST2_P(AUBImageCopyRegionL0, givenImagesWhenCopyingImageRegionThenExpectationsAreMet, ImageSupport) {
    constexpr uint32_t imageDimension = 4;
    constexpr uint32_t numPixels = imageDimension * imageDimension;
    constexpr float origValue = -1.0f;
    constexpr size_t bufferSize = numPixels * sizeof(float);

    const float srcValues[numPixels] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 0.5f, 1.5f, 2.5f,
        3.5f, 4.5f, 5.5f, 6.5f};

    auto srcMemory = static_cast<float *>(this->allocateHostMemory(bufferSize, 0u));
    auto dstInitMemory = static_cast<float *>(this->allocateHostMemory(bufferSize, 0u));
    auto dstOutMemory = static_cast<float *>(this->allocateHostMemory(bufferSize, 0u));
    for (uint32_t i = 0; i < numPixels; i++) {
        srcMemory[i] = srcValues[i];
        dstInitMemory[i] = origValue;
        dstOutMemory[i] = 0.0f;
    }

    const auto format = makeImageAubFormat(ZE_IMAGE_FORMAT_TYPE_FLOAT, 4u, 1u);
    auto srcImage = this->createImage(ZE_IMAGE_TYPE_2D, format, imageDimension, imageDimension, 1u, 0u);
    auto dstImage = this->createImage(ZE_IMAGE_TYPE_2D, format, imageDimension, imageDimension, 1u, 0u);
    ASSERT_NE(nullptr, srcImage);
    ASSERT_NE(nullptr, dstImage);

    const uint32_t srcOffset = std::get<0>(GetParam()).srcOffset;
    const uint32_t dstOffset = std::get<0>(GetParam()).dstOffset;
    const ze_image_region_t srcRegion = {srcOffset, srcOffset, 0, imageDimension / 2, imageDimension / 2, 1};
    const ze_image_region_t dstRegion = {dstOffset, dstOffset, 0, imageDimension / 2, imageDimension / 2, 1};

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, srcImage, srcMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, dstImage, dstInitMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyRegion(cmdList, dstImage, srcImage, &dstRegion, &srcRegion, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, dstOutMemory, dstImage, nullptr, nullptr, 0, nullptr));
    this->synchronize();

    float expectedMemory[numPixels];
    for (uint32_t y = 0; y < imageDimension; y++) {
        for (uint32_t x = 0; x < imageDimension; x++) {
            expectedMemory[y * imageDimension + x] = origValue;
            if (isPixelInImageAubRegion(dstRegion, x, y, 0)) {
                const uint32_t srcX = x - dstOffset + srcOffset;
                const uint32_t srcY = y - dstOffset + srcOffset;
                expectedMemory[y * imageDimension + x] = srcValues[srcY * imageDimension + srcX];
            }
        }
    }

    this->expectImmCmdListMemory(dstOutMemory, expectedMemory, bufferSize);
}

INSTANTIATE_TEST_SUITE_P(AUBImageCopyRegionL0,
                         AUBImageCopyRegionL0,
                         ::testing::Combine(
                             ::testing::ValuesIn(imageAubCopyRegionOffsets),
                             ::testing::Bool()));

} // namespace ult
} // namespace L0
