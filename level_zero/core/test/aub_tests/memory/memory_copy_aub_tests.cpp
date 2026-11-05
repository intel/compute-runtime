/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/memory/memory_aub_fixture.h"

#include <algorithm>
#include <tuple>

namespace L0 {
namespace ult {

using AUBMemoryCopyL0 = MemoryAubParamTest<std::tuple<size_t, size_t>>;

HWTEST_P(AUBMemoryCopyL0, givenDeviceAllocationsAndOffsetsWhenCopyingThenCopiedDataMatchesAndNeighbourMemoryIsUntouched) {
    const float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    const float dstMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    const size_t bufferSize = sizeof(srcMemory);
    const size_t sizeCopied = sizeof(float);
    const auto [srcOffset, dstOffset] = GetParam();

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, srcMemory, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, dstMemory, bufferSize);

    this->appendCopy(ptrOffset(dstBuffer, dstOffset), ptrOffset(srcBuffer, srcOffset), sizeCopied);
    this->executeAndSynchronize();

    float reference[] = {0.0f, 0.0f, 0.0f, 0.0f};
    memcpy(ptrOffset(reference, dstOffset), ptrOffset(srcMemory, srcOffset), sizeCopied);
    this->template expectMemory<FamilyType>(dstBuffer, reference, bufferSize);
}

INSTANTIATE_TEST_SUITE_P(AUBMemoryCopyL0Offsets,
                         AUBMemoryCopyL0,
                         ::testing::Combine(
                             ::testing::Values(0 * sizeof(float), 1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float)),
                             ::testing::Values(0 * sizeof(float), 1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float))));

using AUBMemoryCopyRegionL0 = MemoryAubParamTest<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool>>;

HWTEST_P(AUBMemoryCopyRegionL0, givenDeviceAllocationsAndOriginsWhenCopyingRegionThenDestinationMatchesReference) {
    constexpr uint32_t rowPitch = 20;
    constexpr uint32_t slicePitch = rowPitch * rowPitch;
    constexpr uint32_t elementCount = slicePitch * rowPitch;
    const auto [srcOriginX, srcOriginY, srcOriginZ, dstOriginX, dstOriginY, dstOriginZ, copy3D] = GetParam();

    std::vector<uint8_t> srcMemory(elementCount);
    for (uint32_t i = 0; i < elementCount; i++) {
        srcMemory[i] = static_cast<uint8_t>(i + 1);
    }
    std::vector<uint8_t> reference(elementCount, 0);

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, elementCount);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, elementCount);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, srcMemory.data(), elementCount);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, reference.data(), elementCount);

    const uint32_t regionX = std::min(rowPitch / 2, rowPitch - std::max(srcOriginX, dstOriginX));
    const uint32_t regionY = std::min(rowPitch / 2, rowPitch - std::max(srcOriginY, dstOriginY));
    const uint32_t regionZ = copy3D ? std::min(rowPitch / 2, rowPitch - std::max(srcOriginZ, dstOriginZ)) : 1u;

    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, srcOriginZ, regionX, regionY, regionZ};
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, dstOriginZ, regionX, regionY, regionZ};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(this->commandList->toHandle(),
                                                                     dstBuffer, &dstRegion, rowPitch, slicePitch,
                                                                     srcBuffer, &srcRegion, rowPitch, slicePitch,
                                                                     nullptr, 0, nullptr));
    this->executeAndSynchronize();

    for (uint32_t z = 0; z < regionZ; z++) {
        for (uint32_t y = 0; y < regionY; y++) {
            const auto srcIndex = srcOriginX + (srcOriginY + y) * rowPitch + (srcOriginZ + z) * slicePitch;
            const auto dstIndex = dstOriginX + (dstOriginY + y) * rowPitch + (dstOriginZ + z) * slicePitch;
            memcpy(&reference[dstIndex], &srcMemory[srcIndex], regionX);
        }
    }
    this->template expectMemory<FamilyType>(dstBuffer, reference.data(), elementCount);
}

INSTANTIATE_TEST_SUITE_P(AUBMemoryCopyRegionL0Origins,
                         AUBMemoryCopyRegionL0,
                         ::testing::Combine(
                             ::testing::Values(0u, 3u),
                             ::testing::Values(0u),
                             ::testing::Values(0u, 7u),
                             ::testing::Values(0u, 3u),
                             ::testing::Values(0u),
                             ::testing::Values(0u, 7u),
                             ::testing::Bool()));

} // namespace ult
} // namespace L0
