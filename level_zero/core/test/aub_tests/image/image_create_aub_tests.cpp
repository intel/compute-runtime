/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/image/image_aub_fixture.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

struct AUBImageCreateL0 : public ImageAubTestL0<::testing::Test> {
    void SetUp() override {
        ImageAubFixtureL0::setUp(false, ZE_IMAGE_TYPE_3D);
    }

    void testImageArray(ze_image_type_t imageType) {
        const uint32_t imageHeight = imageType == ZE_IMAGE_TYPE_1DARRAY ? 1u : imageDimension;
        const size_t rowDataSize = imageDimension * pixelSize;
        const size_t hostDataSize = rowDataSize * imageHeight * arrayLevels;

        auto hostMemory = static_cast<uint32_t *>(this->allocateHostMemory(hostDataSize, 0u));
        for (uint32_t i = 0; i < hostDataSize / pixelSize; i++) {
            hostMemory[i] = i;
        }

        auto imageHandle = this->createImage(imageType, imageFormat, imageDimension, imageHeight, 1u, arrayLevels);
        ASSERT_NE(nullptr, imageHandle);

        const auto &imgInfo = Image::fromHandle(imageHandle)->getImageInfo();
        EXPECT_GE(imgInfo.rowPitch, rowDataSize);
        EXPECT_GE(imgInfo.slicePitch, imgInfo.rowPitch);
        EXPECT_GE(imgInfo.size, imgInfo.slicePitch * arrayLevels);

        const size_t readMemorySize = imgInfo.slicePitch * arrayLevels;
        auto readMemory = this->allocateHostBytes(readMemorySize);
        memset(readMemory, 0, readMemorySize);

        auto cmdList = this->getCmdListHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdList, imageHandle, hostMemory, nullptr, nullptr, 0, nullptr));
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemoryExt(cmdList, readMemory, imageHandle, nullptr,
                                                                             static_cast<uint32_t>(imgInfo.rowPitch), static_cast<uint32_t>(imgInfo.slicePitch),
                                                                             nullptr, 0, nullptr));
        this->synchronize();

        std::vector<uint8_t> expectedMemory(readMemorySize, 0u);
        auto hostBytes = reinterpret_cast<const uint8_t *>(hostMemory);
        for (uint32_t array = 0; array < arrayLevels; array++) {
            for (uint32_t row = 0; row < imageHeight; row++) {
                const size_t expectedOffset = array * imgInfo.slicePitch + row * imgInfo.rowPitch;
                const size_t hostOffset = (array * imageHeight + row) * rowDataSize;
                memcpy(&expectedMemory[expectedOffset], &hostBytes[hostOffset], rowDataSize);
            }
        }

        this->expectImmCmdListMemory(readMemory, expectedMemory.data(), readMemorySize);
    }

    static constexpr uint32_t imageDimension = 17;
    static constexpr uint32_t arrayLevels = 10;
    static constexpr uint32_t pixelSize = 4;
    const ze_image_format_t imageFormat = makeImageAubFormat(ZE_IMAGE_FORMAT_TYPE_UNORM, 1u, 4u);
};

HWTEST2_F(AUBImageCreateL0, given1DImageArrayWhenCopyingHostDataInAndOutThenExpectationsAreMet, ImageSupport) {
    this->testImageArray(ZE_IMAGE_TYPE_1DARRAY);
}

HWTEST2_F(AUBImageCreateL0, given2DImageArrayWhenCopyingHostDataInAndOutThenExpectationsAreMet, ImageSupport) {
    this->testImageArray(ZE_IMAGE_TYPE_2DARRAY);
}

HWTEST2_F(AUBImageCreateL0, given2DImageWhenCopyingFromHostMemoryWithDoubledRowPitchThenExpectationsAreMet, ImageSupport) {
    auto imageHandle = this->createImage(ZE_IMAGE_TYPE_2D, imageFormat, imageDimension, imageDimension, 1u, 0u);
    ASSERT_NE(nullptr, imageHandle);

    const auto &imgInfo = Image::fromHandle(imageHandle)->getImageInfo();
    const size_t rowDataSize = imageDimension * pixelSize;
    const size_t inputRowPitch = imgInfo.rowPitch * 2;
    const size_t hostDataSize = inputRowPitch * imageDimension;

    auto hostMemory = this->allocateHostBytes(hostDataSize);
    memset(hostMemory, 0, hostDataSize);
    uint8_t counter = 0;
    for (uint32_t row = 0; row < imageDimension; row++) {
        for (size_t i = 0; i < rowDataSize; i++) {
            hostMemory[row * inputRowPitch + i] = counter++;
        }
    }

    const size_t readMemorySize = rowDataSize * imageDimension;
    auto readMemory = this->allocateHostBytes(readMemorySize);
    memset(readMemory, 0, readMemorySize);

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemoryExt(cmdList, imageHandle, hostMemory, nullptr, static_cast<uint32_t>(inputRowPitch), 0, nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdList, readMemory, imageHandle, nullptr, nullptr, 0, nullptr));
    this->synchronize();

    std::vector<uint8_t> expectedMemory(readMemorySize);
    for (uint32_t row = 0; row < imageDimension; row++) {
        memcpy(&expectedMemory[row * rowDataSize], &hostMemory[row * inputRowPitch], rowDataSize);
    }

    this->expectImmCmdListMemory(readMemory, expectedMemory.data(), readMemorySize);
}

HWTEST2_F(AUBImageCreateL0, given3DImageWhenCopyingFromHostMemoryWithDoubledSlicePitchThenExpectationsAreMet, ImageSupport) {
    auto imageHandle = this->createImage(ZE_IMAGE_TYPE_3D, imageFormat, imageDimension, imageDimension, imageDimension, 0u);
    ASSERT_NE(nullptr, imageHandle);

    const size_t sliceDataSize = imageDimension * imageDimension * pixelSize;
    const size_t computedSlicePitch = imageDimension * alignUp(imageDimension, 4u) * pixelSize;
    const size_t inputSlicePitch = computedSlicePitch * 2;
    const size_t hostDataSize = inputSlicePitch * imageDimension;

    auto hostMemory = this->allocateHostBytes(hostDataSize);
    memset(hostMemory, 0, hostDataSize);
    uint8_t counter = 0;
    for (uint32_t slice = 0; slice < imageDimension; slice++) {
        for (size_t i = 0; i < sliceDataSize; i++) {
            hostMemory[slice * inputSlicePitch + i] = counter++;
        }
    }

    const size_t readMemorySize = computedSlicePitch * imageDimension;
    auto readMemory = this->allocateHostBytes(readMemorySize);
    memset(readMemory, 0, readMemorySize);

    auto cmdList = this->getCmdListHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemoryExt(cmdList, imageHandle, hostMemory, nullptr, 0, static_cast<uint32_t>(inputSlicePitch), nullptr, 0, nullptr));
    ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemoryExt(cmdList, readMemory, imageHandle, nullptr, 0, static_cast<uint32_t>(computedSlicePitch), nullptr, 0, nullptr));
    this->synchronize();

    std::vector<uint8_t> expectedMemory(readMemorySize, 0u);
    for (uint32_t slice = 0; slice < imageDimension; slice++) {
        memcpy(&expectedMemory[slice * computedSlicePitch], &hostMemory[slice * inputSlicePitch], sliceDataSize);
    }

    this->expectImmCmdListMemory(readMemory, expectedMemory.data(), readMemorySize);
}

} // namespace ult
} // namespace L0
