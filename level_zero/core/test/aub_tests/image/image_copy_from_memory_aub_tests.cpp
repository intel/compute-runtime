/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/image/image_aub_fixture.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

using AUBImageCopyFromMemoryL0 = ImageAubCopyEngineTestL0;

HWTEST2_P(AUBImageCopyFromMemoryL0, givenMisalignedHostMemoryWhenCopyingMemoryToImageThenExpectationsAreMet, ImageSupport) {
    for (auto pixelSize : {1u, 2u, 4u}) {
        for (auto offset : {0u, 4u, 8u, 12u}) {
            for (auto width : {3u, 2u, 1u}) {
                const uint32_t imageWidth = 14 / pixelSize;
                const uint32_t imageHeight = 4;
                const size_t imageRowPitch = imageWidth * pixelSize;
                const size_t imageDataSize = imageRowPitch * imageHeight;
                constexpr size_t unalignedOffset = 4;

                auto zeroMemory = this->allocateHostBytes(imageDataSize);
                memset(zeroMemory, 0, imageDataSize);

                auto srcAllocation = this->allocateHostBytes(unalignedOffset + imageDataSize);
                memset(srcAllocation, 0, unalignedOffset + imageDataSize);
                auto srcMemoryUnaligned = ptrOffset(srcAllocation, unalignedOffset);
                for (size_t i = 0; i < imageDataSize; i++) {
                    srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
                }

                auto readMemory = this->allocateHostBytes(imageDataSize);
                memset(readMemory, 0xFF, imageDataSize);

                auto image = this->createImage(ZE_IMAGE_TYPE_2D, makeImageAubFormat(ZE_IMAGE_FORMAT_TYPE_UINT, 1u, pixelSize), imageWidth, imageHeight, 1u, 0u);
                ASSERT_NE(nullptr, image);

                const uint32_t originY = 1;
                const ze_image_region_t region = {0, originY, 0, width, 1, 1};

                auto cmdList = this->getCmdListHandle();
                ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, zeroMemory, nullptr, nullptr, 0, nullptr));
                ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, ptrOffset(srcMemoryUnaligned, offset), &region, nullptr, 0, nullptr));
                ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, readMemory, image, nullptr, nullptr, 0, nullptr));
                this->synchronize();

                std::vector<uint8_t> expectedMemory(imageDataSize, 0u);
                memcpy(&expectedMemory[imageRowPitch * originY], &srcMemoryUnaligned[offset], width * pixelSize);
                this->expectImmCmdListMemory(readMemory, expectedMemory.data(), imageDataSize);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(AUBImageCopyFromMemoryL0,
                         AUBImageCopyFromMemoryL0,
                         ::testing::Bool());

using AUBImageCopyFromMemoryParamL0 = ImageAubUnalignedCopyTestL0;

HWTEST2_P(AUBImageCopyFromMemoryParamL0, givenUnalignedHostMemoryWhenCopyingMemoryToImageRegionThenExpectationsAreMet, ImageSupport) {
    this->createImageWithRegion();
    ASSERT_NE(nullptr, this->image);

    auto initMemory = this->allocateHostBytes(this->imageDataSize);
    memset(initMemory, 0xFF, this->imageDataSize);

    auto srcAllocation = this->allocateHostBytes(unalignedOffset + this->imageDataSize);
    auto srcMemoryUnaligned = ptrOffset(srcAllocation, unalignedOffset);
    for (size_t i = 0; i < this->imageDataSize; i++) {
        srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
    }

    auto readMemory = this->allocateHostBytes(this->imageDataSize);
    memset(readMemory, 0, this->imageDataSize);

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, this->image, initMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemoryExt(cmdList, this->image, srcMemoryUnaligned, &this->region, this->rowPitch, this->slicePitch, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, readMemory, this->image, nullptr, nullptr, 0, nullptr));
    this->synchronize();

    std::vector<uint8_t> expectedMemory(this->imageDataSize, 0xFF);
    for (uint32_t z = 0; z < this->imageDepth; z++) {
        for (uint32_t y = 0; y < this->imageHeight; y++) {
            for (uint32_t x = 0; x < this->imageWidth; x++) {
                if (isPixelInImageAubRegion(this->region, x, y, z)) {
                    const size_t srcOffset = (z - this->region.originZ) * this->slicePitch + (y - this->region.originY) * this->rowPitch + (x - this->region.originX) * this->elementSize;
                    const size_t dstOffset = z * this->slicePitch + y * this->rowPitch + x * this->elementSize;
                    memcpy(&expectedMemory[dstOffset], &srcMemoryUnaligned[srcOffset], this->elementSize);
                }
            }
        }
    }

    this->expectImmCmdListMemory(readMemory, expectedMemory.data(), this->imageDataSize);
}

INSTANTIATE_TEST_SUITE_P(AUBImageCopyFromMemoryL0,
                         AUBImageCopyFromMemoryParamL0,
                         ::testing::ValuesIn(getImageAubUnalignedCopyParams()));

} // namespace ult
} // namespace L0
