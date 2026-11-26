/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/image/image_aub_fixture.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

struct AUBImageVerifyMemoryL0 : public ImageAubTestL0<::testing::Test> {
    void SetUp() override {
        ImageAubFixtureL0::setUp(false, ZE_IMAGE_TYPE_3D);
    }
};

HWTEST2_F(AUBImageVerifyMemoryL0, givenImageRegionWrittenWhenVerifyingEveryPixelThenEqualAndNotEqualExpectationsAreMet, ImageSupport) {
    constexpr uint32_t imageWidth = 10;
    constexpr uint32_t imageHeight = 19;
    constexpr uint32_t imageDepth = 7;
    constexpr size_t elementSize = 16;
    constexpr size_t imageDataSize = imageWidth * imageHeight * imageDepth * elementSize;
    const uint32_t regionValues[] = {0x3f800000, 0x00000000, 0x3f555555, 0x3f2aaaaa};
    uint8_t initValues[elementSize];
    memset(initValues, 0xAB, elementSize);

    constexpr uint32_t origin[3] = {1, 2, 3};
    const ze_image_region_t region = {origin[0], origin[1], origin[2],
                                      imageWidth - origin[0], imageHeight - origin[1], imageDepth - origin[2]};

    auto initMemory = this->allocateHostBytes(imageDataSize);
    memset(initMemory, 0xAB, imageDataSize);

    auto regionMemory = this->allocateHostBytes(imageDataSize);
    for (size_t pixel = 0; pixel < imageDataSize / elementSize; pixel++) {
        memcpy(&regionMemory[pixel * elementSize], regionValues, elementSize);
    }

    auto readMemory = this->allocateHostBytes(imageDataSize);
    memset(readMemory, 0, imageDataSize);

    auto image = this->createImage(ZE_IMAGE_TYPE_3D, makeImageAubFormat(ZE_IMAGE_FORMAT_TYPE_UINT, 4u, 4u), imageWidth, imageHeight, imageDepth, 0u);
    ASSERT_NE(nullptr, image);

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, initMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, regionMemory, &region, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, readMemory, image, nullptr, nullptr, 0, nullptr));
    this->synchronize();

    std::vector<uint8_t> expectedMemory(imageDataSize);
    for (uint32_t z = 0; z < imageDepth; z++) {
        for (uint32_t y = 0; y < imageHeight; y++) {
            for (uint32_t x = 0; x < imageWidth; x++) {
                const size_t pixelOffset = ((z * imageHeight + y) * imageWidth + x) * elementSize;
                const void *validData = isPixelInImageAubRegion(region, x, y, z) ? static_cast<const void *>(regionValues) : static_cast<const void *>(initValues);
                memcpy(&expectedMemory[pixelOffset], validData, elementSize);
            }
        }
    }
    this->expectImmCmdListMemory(readMemory, expectedMemory.data(), imageDataSize);

    if (NEO::UnitTestHelper<FamilyType>::isExpectMemoryNotEqualSupported()) {
        auto pixelData = readMemory;
        for (uint32_t z = 0; z < imageDepth; z++) {
            for (uint32_t y = 0; y < imageHeight; y++) {
                for (uint32_t x = 0; x < imageWidth; x++) {
                    const void *invalidData = isPixelInImageAubRegion(region, x, y, z) ? static_cast<const void *>(initValues) : static_cast<const void *>(regionValues);
                    this->expectImmCmdListMemoryNotEqual(pixelData, invalidData, elementSize);
                    pixelData += elementSize;
                }
            }
        }
    }
}

} // namespace ult
} // namespace L0
