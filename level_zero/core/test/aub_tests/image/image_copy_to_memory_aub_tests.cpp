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

using AUBImageCopyToMemoryL0 = ImageAubCopyEngineTestL0;

HWTEST2_P(AUBImageCopyToMemoryL0, givenMisalignedHostMemoryWhenCopyingImageToMemoryThenExpectationsAreMet, ImageSupport) {
    for (auto pixelSize : {1u, 2u, 4u}) {
        const uint32_t imageWidth = 14 / pixelSize;
        const uint32_t imageHeight = 4;
        const size_t imageRowPitch = imageWidth * pixelSize;
        const size_t imageDataSize = imageRowPitch * imageHeight;

        auto srcMemory = this->allocateHostBytes(imageDataSize);
        for (size_t i = 0; i < imageDataSize; i++) {
            srcMemory[i] = static_cast<uint8_t>(i);
        }

        auto image = this->createImage(ZE_IMAGE_TYPE_2D, makeImageAubFormat(ZE_IMAGE_FORMAT_TYPE_UINT, 1u, pixelSize), imageWidth, imageHeight, 1u, 0u);
        ASSERT_NE(nullptr, image);

        auto cmdList = this->getCmdListHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, image, srcMemory, nullptr, nullptr, 0, nullptr));

        struct MisalignedCopyCase {
            uint8_t *dstMemory;
            uint32_t offset;
            uint32_t width;
        };
        std::vector<MisalignedCopyCase> copyCases;

        const uint32_t originY = 1;
        for (auto offset : {0u, 4u, 8u, 12u}) {
            for (auto width : {3u, 2u, 1u}) {
                auto dstMemory = this->allocateHostBytes(imageDataSize);
                memset(dstMemory, 0, imageDataSize);
                copyCases.push_back({dstMemory, offset, width});

                const ze_image_region_t region = {0, originY, 0, width, 1, 1};
                ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, ptrOffset(dstMemory, offset), image, &region, nullptr, 0, nullptr));
            }
        }
        this->synchronize();

        for (const auto &copyCase : copyCases) {
            std::vector<uint8_t> expectedMemory(imageDataSize, 0u);
            memcpy(&expectedMemory[copyCase.offset], &srcMemory[imageRowPitch * originY], copyCase.width * pixelSize);
            this->expectImmCmdListMemory(copyCase.dstMemory, expectedMemory.data(), imageDataSize);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(AUBImageCopyToMemoryL0,
                         AUBImageCopyToMemoryL0,
                         ::testing::Bool());

using AUBImageCopyToMemoryParamL0 = ImageAubUnalignedCopyTestL0;

HWTEST2_P(AUBImageCopyToMemoryParamL0, givenUnalignedHostMemoryWhenCopyingImageRegionToMemoryThenExpectationsAreMet, ImageSupport) {
    this->createImageWithRegion();
    ASSERT_NE(nullptr, this->image);

    auto srcMemory = this->allocateHostBytes(this->imageDataSize);
    for (size_t i = 0; i < this->imageDataSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(i);
    }

    auto dstAllocation = this->allocateHostBytes(unalignedOffset + this->imageDataSize);
    memset(dstAllocation, 0xFF, unalignedOffset + this->imageDataSize);
    auto dstMemoryUnaligned = ptrOffset(dstAllocation, unalignedOffset);

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, this->image, srcMemory, nullptr, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemoryExt(cmdList, dstMemoryUnaligned, this->image, &this->region, this->rowPitch, this->slicePitch, nullptr, 0, nullptr));
    this->synchronize();

    std::vector<uint8_t> expectedMemory(unalignedOffset + this->imageDataSize, 0xFF);
    auto expectedRegionData = ptrOffset(expectedMemory.data(), unalignedOffset);
    for (uint32_t z = 0; z < this->region.depth; z++) {
        for (uint32_t y = 0; y < this->region.height; y++) {
            const size_t srcRowOffset = (this->region.originZ + z) * this->slicePitch + (this->region.originY + y) * this->rowPitch + this->region.originX * this->elementSize;
            const size_t dstRowOffset = z * this->slicePitch + y * this->rowPitch;
            memcpy(&expectedRegionData[dstRowOffset], &srcMemory[srcRowOffset], this->region.width * this->elementSize);
        }
    }

    this->expectImmCmdListMemory(dstAllocation, expectedMemory.data(), expectedMemory.size());
}

INSTANTIATE_TEST_SUITE_P(AUBImageCopyToMemoryL0,
                         AUBImageCopyToMemoryParamL0,
                         ::testing::ValuesIn(getImageAubUnalignedCopyParams()));

} // namespace ult
} // namespace L0
