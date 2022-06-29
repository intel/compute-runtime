/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "patch_shared.h"

using namespace NEO;

template <bool localIdX = true, bool localIdY = true, bool localIdZ = true, bool flattenedId = false>
struct PerThreadDataTests : public ClDeviceFixture,
                            ::testing::Test {

    void SetUp() override {
        ClDeviceFixture::SetUp();

        kernelInfo.setLocalIds({localIdX, localIdY, localIdZ});
        kernelInfo.kernelDescriptor.kernelAttributes.flags.usesFlattenedLocalIds = flattenedId;
        kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent = !(localIdX || localIdY || localIdZ || flattenedId);

        numChannels = kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;
        simd = 32;
        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = simd;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.KernelHeapSize = sizeof(kernelIsa);

        grfSize = 32;
        indirectHeapMemorySize = 4096;
        indirectHeapMemory = reinterpret_cast<uint8_t *>(alignedMalloc(indirectHeapMemorySize, 32));
        ASSERT_TRUE(isAligned<32>(indirectHeapMemory));
    }

    void TearDown() override {
        alignedFree(indirectHeapMemory);
        ClDeviceFixture::TearDown();
    }

    const std::array<uint8_t, 3> workgroupWalkOrder = {{0, 1, 2}};
    uint32_t simd;
    uint32_t grfSize;
    uint32_t numChannels;
    uint32_t kernelIsa[32];
    uint8_t *indirectHeapMemory;
    size_t indirectHeapMemorySize;

    SKernelBinaryHeaderCommon kernelHeader;
    MockKernelInfo kernelInfo;
};

typedef PerThreadDataTests<> PerThreadDataXYZTests;

HWTEST_F(PerThreadDataXYZTests, WhenGettingLocalIdSizePerThreadThenCorrectValueIsReturned) {
    EXPECT_EQ(3 * 2 * grfSize, PerThreadDataHelper::getLocalIdSizePerThread(simd, grfSize, numChannels));
}

HWTEST_F(PerThreadDataXYZTests, WhenGettingPerThreadDataSizeTotalThenCorrectValueIsReturned) {
    size_t localWorkSize = 256;
    EXPECT_EQ(256 * 3 * 2 * grfSize / 32, PerThreadDataHelper::getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize));
}

HWTEST_F(PerThreadDataXYZTests, Given256x1x1WhenSendingPerThreadDataThenCorrectAmountOfIndirectHeapIsConsumed) {
    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    const std::array<uint16_t, 3> localWorkSizes = {{256, 1, 1}};
    size_t localWorkSize = localWorkSizes[0] * localWorkSizes[1] * localWorkSizes[2];

    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
        grfSize,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);
    auto expectedPerThreadDataSizeTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize);

    size_t sizeConsumed = indirectHeap.getUsed() - offsetPerThreadData;
    EXPECT_EQ(expectedPerThreadDataSizeTotal, sizeConsumed);
}

HWTEST_F(PerThreadDataXYZTests, Given2x4x8WhenSendingPerThreadDataThenCorrectAmountOfIndirectHeapIsConsumed) {
    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    const std::array<uint16_t, 3> localWorkSizes = {{2, 4, 8}};
    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
        grfSize,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);

    size_t sizeConsumed = indirectHeap.getUsed() - offsetPerThreadData;
    EXPECT_EQ(64u * (3u * 2u * 4u * 8u) / 32u, sizeConsumed);
}

HWTEST_F(PerThreadDataXYZTests, GivenDifferentSimdWhenGettingThreadPayloadSizeThenCorrectSizeIsReturned) {
    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 2u * 3u, size);

    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 3u, size);

    kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent = true;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 4u, size);

    kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent = true;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 5u, size);
}

typedef PerThreadDataTests<false, false, false, false> PerThreadDataNoIdsTests;

HWTEST_F(PerThreadDataNoIdsTests, givenZeroChannelsWhenPassedToGetLocalIdSizePerThreadThenSizeOfOneGrfIsReturned) {
    EXPECT_EQ(32u, PerThreadDataHelper::getLocalIdSizePerThread(simd, grfSize, numChannels));
}

HWTEST_F(PerThreadDataNoIdsTests, givenZeroChannelsAndHighWkgSizeWhenGetPerThreadDataSizeTotalIsCalledThenReturnedSizeContainsUnusedGrfPerEachThread) {
    size_t localWorkSize = 256u;
    auto threadCount = localWorkSize / simd;
    auto expectedSize = threadCount * grfSize;
    EXPECT_EQ(expectedSize, PerThreadDataHelper::getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize));
}

HWTEST_F(PerThreadDataNoIdsTests, GivenThreadPaylodDataWithoutLocalIdsWhenSendingPerThreadDataThenIndirectHeapMemoryIsNotConsumed) {
    uint8_t fillValue = 0xcc;
    memset(indirectHeapMemory, fillValue, indirectHeapMemorySize);

    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    const std::array<uint16_t, 3> localWorkSizes = {{256, 1, 1}};
    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
        grfSize,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);

    size_t sizeConsumed = indirectHeap.getUsed() - offsetPerThreadData;
    EXPECT_EQ(0u, sizeConsumed);

    size_t i = 0;
    while (i < indirectHeapMemorySize) {
        ASSERT_EQ(fillValue, indirectHeapMemory[i]) << "for index " << i;
        ++i;
    }
}

HWTEST_F(PerThreadDataNoIdsTests, GivenSimdWhenGettingThreadPayloadSizeThenCorrectValueIsReturned) {
    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize, size);

    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize, size);

    kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent = true;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 2u, size);
}

typedef PerThreadDataTests<false, false, false, true> PerThreadDataFlattenedIdsTests;

HWTEST_F(PerThreadDataFlattenedIdsTests, GivenSimdWhenGettingThreadPayloadSizeThenCorrectValueIsReturned) {
    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 2u, size);

    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize, size);

    kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataHeaderIsPresent = true;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 2u, size);

    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    size = PerThreadDataHelper::getThreadPayloadSize(kernelInfo.kernelDescriptor, grfSize);
    EXPECT_EQ(grfSize * 3u, size);
}

TEST(PerThreadDataTest, WhenSettingLocalIdsInPerThreadDataThenIdsAreSetInCorrectOrder) {
    uint32_t simd = 8;
    uint32_t grfSize = 32;
    uint32_t numChannels = 3;
    uint32_t localWorkSize = 24;

    const std::array<uint16_t, 3> localWorkSizes = {{24, 1, 1}};
    const std::array<uint8_t, 3> workgroupWalkOrder = {{0, 1, 2}};

    auto sizePerThreadDataTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize);

    auto sizeOverSizedBuffer = sizePerThreadDataTotal * 4;
    auto buffer = static_cast<char *>(alignedMalloc(sizeOverSizedBuffer, 16));
    memset(buffer, 0, sizeOverSizedBuffer);

    // Setup reference filled with zeros
    auto reference = static_cast<char *>(alignedMalloc(sizePerThreadDataTotal, 16));
    memset(reference, 0, sizePerThreadDataTotal);

    LinearStream stream(buffer, sizeOverSizedBuffer / 2);
    PerThreadDataHelper::sendPerThreadData(
        stream,
        simd,
        grfSize,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);

    // Check if buffer overrun happend, only first sizePerThreadDataTotal bytes can be overwriten, following should be same as reference.
    for (auto i = sizePerThreadDataTotal; i < sizeOverSizedBuffer; i += sizePerThreadDataTotal) {
        int result = memcmp(buffer + i, reference, sizePerThreadDataTotal);
        EXPECT_EQ(0, result);
    }

    alignedFree(buffer);
    alignedFree(reference);
}

TEST(PerThreadDataTest, givenSimdEqualOneWhenSettingLocalIdsInPerThreadDataThenIdsAreSetInCorrectOrder) {
    uint32_t simd = 1;
    uint32_t grfSize = 32;
    uint32_t numChannels = 3;
    uint32_t localWorkSize = 24;

    const std::array<uint16_t, 3> localWorkSizes = {{3, 4, 2}};
    const std::array<uint8_t, 3> workgroupWalkOrder = {{0, 1, 2}};

    auto sizePerThreadDataTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(simd, grfSize, numChannels, localWorkSize);

    auto sizeOverSizedBuffer = sizePerThreadDataTotal * 4;
    auto buffer = static_cast<char *>(alignedMalloc(sizeOverSizedBuffer, 16));
    memset(buffer, 0, sizeOverSizedBuffer);

    // Setup reference filled with zeros
    auto reference = static_cast<char *>(alignedMalloc(sizePerThreadDataTotal, 16));
    memset(reference, 0, sizePerThreadDataTotal);

    LinearStream stream(buffer, sizeOverSizedBuffer / 2);
    PerThreadDataHelper::sendPerThreadData(
        stream,
        simd,
        grfSize,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);

    auto bufferPtr = buffer;
    for (uint16_t i = 0; i < localWorkSizes[2]; i++) {
        for (uint16_t j = 0; j < localWorkSizes[1]; j++) {
            for (uint16_t k = 0; k < localWorkSizes[0]; k++) {
                uint16_t ids[] = {k, j, i};
                int result = memcmp(bufferPtr, ids, sizeof(uint16_t) * 3);
                EXPECT_EQ(0, result);
                bufferPtr += grfSize;
            }
        }
    }
    // Check if buffer overrun happend, only first sizePerThreadDataTotal bytes can be overwriten, following should be same as reference.
    for (auto i = sizePerThreadDataTotal; i < sizeOverSizedBuffer; i += sizePerThreadDataTotal) {
        int result = memcmp(buffer + i, reference, sizePerThreadDataTotal);
        EXPECT_EQ(0, result);
    }

    alignedFree(buffer);
    alignedFree(reference);
}
