/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/memory/memory_aub_fixture.h"

#include <tuple>

namespace L0 {
namespace ult {

namespace {
constexpr size_t unalignedOffsets[] = {0, 1, 2, 3};
constexpr size_t unalignedSizes[] = {4, 3, 2, 1};
constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr size_t alphabetSize = sizeof(alphabet);
constexpr uint32_t regionWidth = 10;
constexpr uint32_t regionSlicePitch = regionWidth * regionWidth;
constexpr size_t regionVolume = regionSlicePitch * regionWidth;
constexpr size_t regionBufferSize = MemoryConstants::pageSize;
} // namespace

using AUBReadMemoryL0 = MemoryAubParamTest<std::tuple<size_t, AubMemoryType>>;

HWTEST_P(AUBReadMemoryL0, givenDeviceAllocationWhenCopyingToHostMemoryThenCopiedDataMatchesAndNeighbourMemoryIsUntouched) {
    const float srcMemory[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float reference[] = {0.0f, 0.0f, 0.0f, 0.0f};
    const size_t bufferSize = sizeof(srcMemory);
    const size_t sizeCopied = sizeof(float);
    const auto [offset, dstType] = GetParam();

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    auto dstBuffer = this->allocate(dstType, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, srcMemory, bufferSize);
    this->initialize(dstType, dstBuffer, reference, bufferSize);

    this->appendCopy(dstBuffer, ptrOffset(srcBuffer, offset), sizeCopied);
    this->executeAndSynchronize();

    memcpy(reference, ptrOffset(srcMemory, offset), sizeCopied);
    this->template expectMemory<FamilyType>(this->getGpuPointer(dstBuffer), reference, bufferSize);
}

INSTANTIATE_TEST_SUITE_P(AUBReadMemoryL0Offsets,
                         AUBReadMemoryL0,
                         ::testing::Combine(
                             ::testing::Values(0 * sizeof(float), 1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float)),
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::importedSystem)));

using AUBReadMemoryUnalignedL0 = MemoryAubParamTest<AubMemoryType>;

HWTEST_P(AUBReadMemoryUnalignedL0, givenUnalignedOffsetAndSizeWhenCopyingToHostMemoryThenCopiedDataMatches) {
    const auto dstType = GetParam();
    const char zeroMemory[alphabetSize] = {};

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, alphabetSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, alphabet, alphabetSize);

    for (auto offset : unalignedOffsets) {
        for (auto size : unalignedSizes) {
            auto dstBuffer = this->allocate(dstType, alphabetSize);
            this->initialize(dstType, dstBuffer, zeroMemory, alphabetSize);

            this->appendCopy(ptrOffset(dstBuffer, offset), ptrOffset(srcBuffer, offset), size);
            this->executeAndSynchronize();

            this->template expectMemory<FamilyType>(ptrOffset(this->getGpuPointer(dstBuffer), offset), ptrOffset(alphabet, offset), size);
            this->commandList->reset();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(HostTypes,
                         AUBReadMemoryUnalignedL0,
                         ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::importedSystem));

using AUBReadMemoryRegionL0 = MemoryAubParamTest<std::tuple<uint32_t, uint32_t, AubMemoryType>>;

HWTEST_P(AUBReadMemoryRegionL0, givenSliceOffsetsWhenCopyingRegionToHostMemoryThenOnlyTargetSliceIsWritten) {
    const auto [zBufferOffset, zHostOffset, dstType] = GetParam();

    std::vector<uint8_t> srcMemory(regionBufferSize);
    for (size_t i = 0; i < regionBufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(i & 0xff);
    }
    std::vector<uint8_t> reference(regionBufferSize, 0);

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, regionBufferSize);
    auto dstBuffer = this->allocate(dstType, regionBufferSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, srcMemory.data(), regionBufferSize);
    this->initialize(dstType, dstBuffer, reference.data(), regionBufferSize);

    const ze_copy_region_t srcRegion = {0, 0, zBufferOffset, regionWidth, regionWidth, 1};
    const ze_copy_region_t dstRegion = {0, 0, zHostOffset, regionWidth, regionWidth, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(this->commandList->toHandle(),
                                                                     dstBuffer, &dstRegion, regionWidth, regionSlicePitch,
                                                                     srcBuffer, &srcRegion, regionWidth, regionSlicePitch,
                                                                     nullptr, 0, nullptr));
    this->executeAndSynchronize();

    memcpy(&reference[regionSlicePitch * zHostOffset], &srcMemory[regionSlicePitch * zBufferOffset], regionSlicePitch);
    this->template expectMemory<FamilyType>(this->getGpuPointer(dstBuffer), reference.data(), regionVolume);
}

INSTANTIATE_TEST_SUITE_P(AUBReadMemoryRegionL0Slices,
                         AUBReadMemoryRegionL0,
                         ::testing::Combine(
                             ::testing::Values(0u, 1u, 2u, 3u, 4u),
                             ::testing::Values(0u, 1u, 2u, 3u, 4u),
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::importedSystem)));

using AUBReadMemoryRegionUnalignedL0 = AUBReadMemoryUnalignedL0;

HWTEST_P(AUBReadMemoryRegionUnalignedL0, givenMisalignedHostPtrWhenCopyingRegionToHostThenDataMatchesAndNeighboursAreUntouched) {
    const auto dstType = GetParam();
    const char zeroMemory[alphabetSize] = {};
    constexpr uint32_t rowPitch = alphabetSize / 4;
    constexpr uint32_t slicePitch = 4 * rowPitch;
    constexpr uint32_t srcOriginY = 1;

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, alphabetSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, alphabet, alphabetSize);

    for (auto offset : unalignedOffsets) {
        for (auto size : unalignedSizes) {
            auto dstBuffer = this->allocate(dstType, alphabetSize);
            this->initialize(dstType, dstBuffer, zeroMemory, alphabetSize);

            const ze_copy_region_t srcRegion = {0, srcOriginY, 0, static_cast<uint32_t>(size), 1, 1};
            const ze_copy_region_t dstRegion = {0, 0, 0, static_cast<uint32_t>(size), 1, 1};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(this->commandList->toHandle(),
                                                                             ptrOffset(dstBuffer, offset), &dstRegion, rowPitch, slicePitch,
                                                                             srcBuffer, &srcRegion, rowPitch, slicePitch,
                                                                             nullptr, 0, nullptr));
            this->executeAndSynchronize();

            char reference[alphabetSize] = {};
            memcpy(&reference[offset], &alphabet[rowPitch * srcOriginY], size);
            this->template expectMemory<FamilyType>(this->getGpuPointer(dstBuffer), reference, alphabetSize);
            this->commandList->reset();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(HostTypes,
                         AUBReadMemoryRegionUnalignedL0,
                         ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::importedSystem));

using AUBWriteMemoryL0 = AUBReadMemoryL0;

HWTEST_P(AUBWriteMemoryL0, givenHostMemoryWhenCopyingToDeviceAllocationThenCopiedDataMatchesAndNeighbourMemoryIsUntouched) {
    constexpr size_t elementCount = 1024;
    constexpr size_t bufferSize = elementCount * sizeof(float);
    const size_t sizeCopied = sizeof(float);
    const auto [offset, srcType] = GetParam();

    std::vector<float> srcMemory(elementCount);
    for (size_t i = 0; i < elementCount; i++) {
        srcMemory[i] = static_cast<float>(i) + 1.0f;
    }
    std::vector<float> reference(elementCount, 0.0f);

    auto srcBuffer = this->allocate(srcType, bufferSize);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    this->initialize(srcType, srcBuffer, srcMemory.data(), bufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, reference.data(), bufferSize);

    this->appendCopy(ptrOffset(dstBuffer, offset), srcBuffer, sizeCopied);
    this->executeAndSynchronize();

    memcpy(ptrOffset(reference.data(), offset), srcMemory.data(), sizeCopied);
    this->template expectMemory<FamilyType>(dstBuffer, reference.data(), bufferSize);
}

INSTANTIATE_TEST_SUITE_P(AUBWriteMemoryL0Offsets,
                         AUBWriteMemoryL0,
                         ::testing::Combine(
                             ::testing::Values(0 * sizeof(float), 1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float)),
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::system)));

using AUBWriteMemoryUnalignedL0 = AUBReadMemoryUnalignedL0;

HWTEST_P(AUBWriteMemoryUnalignedL0, givenUnalignedOffsetAndSizeWhenCopyingToDeviceAllocationThenCopiedDataMatches) {
    const auto srcType = GetParam();
    const char zeroMemory[alphabetSize] = {};

    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, alphabetSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, zeroMemory, alphabetSize);

    for (auto offset : unalignedOffsets) {
        for (auto size : unalignedSizes) {
            auto srcBuffer = this->allocate(srcType, alphabetSize);
            this->initialize(srcType, srcBuffer, alphabet, alphabetSize);

            this->appendCopy(ptrOffset(dstBuffer, offset), ptrOffset(srcBuffer, offset), size);
            this->executeAndSynchronize();

            this->template expectMemory<FamilyType>(ptrOffset(dstBuffer, offset), ptrOffset(alphabet, offset), size);
            this->commandList->reset();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(HostTypes,
                         AUBWriteMemoryUnalignedL0,
                         ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::system));

using AUBWriteMemoryRegionL0 = AUBReadMemoryRegionL0;

HWTEST_P(AUBWriteMemoryRegionL0, givenSliceOffsetsWhenCopyingRegionFromHostMemoryThenOnlyTargetSliceIsWritten) {
    const auto [zBufferOffset, zHostOffset, srcType] = GetParam();

    std::vector<uint8_t> srcMemory(regionBufferSize);
    for (size_t i = 0; i < regionBufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(i & 0xff);
    }
    std::vector<uint8_t> reference(regionBufferSize, 0);

    auto srcBuffer = this->allocate(srcType, regionBufferSize);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, regionBufferSize);
    this->initialize(srcType, srcBuffer, srcMemory.data(), regionBufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, reference.data(), regionBufferSize);

    const ze_copy_region_t srcRegion = {0, 0, zHostOffset, regionWidth, regionWidth, 1};
    const ze_copy_region_t dstRegion = {0, 0, zBufferOffset, regionWidth, regionWidth, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(this->commandList->toHandle(),
                                                                     dstBuffer, &dstRegion, regionWidth, regionSlicePitch,
                                                                     srcBuffer, &srcRegion, regionWidth, regionSlicePitch,
                                                                     nullptr, 0, nullptr));
    this->executeAndSynchronize();

    memcpy(&reference[regionSlicePitch * zBufferOffset], &srcMemory[regionSlicePitch * zHostOffset], regionSlicePitch);
    this->template expectMemory<FamilyType>(dstBuffer, reference.data(), regionVolume);
}

INSTANTIATE_TEST_SUITE_P(AUBWriteMemoryRegionL0Slices,
                         AUBWriteMemoryRegionL0,
                         ::testing::Combine(
                             ::testing::Values(0u, 1u, 2u, 3u, 4u),
                             ::testing::Values(0u, 1u, 2u, 3u, 4u),
                             ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::system)));

using AUBWriteMemoryRegionUnalignedL0 = AUBReadMemoryUnalignedL0;

HWTEST_P(AUBWriteMemoryRegionUnalignedL0, givenMisalignedHostPtrWhenCopyingRegionToDeviceThenDataMatchesAndNeighboursAreUntouched) {
    const auto srcType = GetParam();
    const char zeroMemory[alphabetSize] = {};
    constexpr uint32_t rowPitch = alphabetSize / 4;
    constexpr uint32_t slicePitch = 4 * rowPitch;
    constexpr uint32_t dstOriginY = 1;
    for (auto offset : unalignedOffsets) {
        for (auto size : unalignedSizes) {
            auto srcBuffer = this->allocate(srcType, alphabetSize);
            auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, alphabetSize);
            this->initialize(srcType, srcBuffer, alphabet, alphabetSize);
            this->initialize(AubMemoryType::deviceUsm, dstBuffer, zeroMemory, alphabetSize);

            const ze_copy_region_t srcRegion = {0, 0, 0, static_cast<uint32_t>(size), 1, 1};
            const ze_copy_region_t dstRegion = {0, dstOriginY, 0, static_cast<uint32_t>(size), 1, 1};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopyRegion(this->commandList->toHandle(),
                                                                             dstBuffer, &dstRegion, rowPitch, slicePitch,
                                                                             ptrOffset(srcBuffer, offset), &srcRegion, rowPitch, slicePitch,
                                                                             nullptr, 0, nullptr));
            this->executeAndSynchronize();

            char reference[alphabetSize] = {};
            memcpy(&reference[rowPitch * dstOriginY], &alphabet[offset], size);
            this->template expectMemory<FamilyType>(dstBuffer, reference, alphabetSize);
            this->commandList->reset();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(HostTypes,
                         AUBWriteMemoryRegionUnalignedL0,
                         ::testing::Values(AubMemoryType::hostUsm, AubMemoryType::system));

using AUBWriteCopyReadMemoryL0 = Test<MemoryAubFixture>;

HWTEST_F(AUBWriteCopyReadMemoryL0, givenTwoDeviceAllocationsWhenWritingCopyingAndReadingBackThenDataValidates) {
    const char srcMemory[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const char dstMemory[] = {11, 12, 13, 14, 15, 16, 17, 18};
    constexpr size_t bufferSize = sizeof(srcMemory);

    auto srcBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, srcBuffer, srcMemory, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, dstMemory, bufferSize);
    this->executeAndSynchronize();
    this->commandList->reset();

    this->template expectMemory<FamilyType>(srcBuffer, srcMemory, bufferSize);
    this->template expectMemory<FamilyType>(dstBuffer, dstMemory, bufferSize);

    auto hostPtrMemory = this->allocate(AubMemoryType::system, bufferSize);
    memset(hostPtrMemory, 0, bufferSize);
    this->appendCopy(dstBuffer, srcBuffer, bufferSize);
    this->appendBarrier();
    this->appendCopy(hostPtrMemory, dstBuffer, bufferSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(dstBuffer, srcMemory, bufferSize);
    this->template expectMemory<FamilyType>(this->getGpuPointer(hostPtrMemory), srcMemory, bufferSize);
}

} // namespace ult
} // namespace L0
