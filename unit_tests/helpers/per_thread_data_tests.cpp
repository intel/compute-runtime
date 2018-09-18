/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/per_thread_data.h"
#include "runtime/program/kernel_info.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "patch_shared.h"

using namespace OCLRT;

template <bool localIdX = true, bool localIdY = true, bool localIdZ = true, bool flattenedId = false>
struct PerThreadDataTests : public DeviceFixture,
                            ::testing::Test {

    void SetUp() override {
        DeviceFixture::SetUp();

        kernelHeader = {};
        kernelHeader.KernelHeapSize = sizeof(kernelIsa);

        threadPayload = {};
        threadPayload.LocalIDXPresent = localIdX ? 1 : 0;
        threadPayload.LocalIDYPresent = localIdY ? 1 : 0;
        threadPayload.LocalIDZPresent = localIdZ ? 1 : 0;
        threadPayload.LocalIDFlattenedPresent = flattenedId;
        threadPayload.UnusedPerThreadConstantPresent =
            !(localIdX || localIdY || localIdZ || flattenedId);

        executionEnvironment = {};
        executionEnvironment.CompiledSIMD32 = 1;
        executionEnvironment.LargestCompiledSIMDSize = 32;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;

        simd = executionEnvironment.LargestCompiledSIMDSize;
        numChannels = threadPayload.LocalIDXPresent +
                      threadPayload.LocalIDYPresent +
                      threadPayload.LocalIDZPresent;

        indirectHeapMemorySize = 4096;
        indirectHeapMemory = reinterpret_cast<uint8_t *>(alignedMalloc(indirectHeapMemorySize, 32));
        ASSERT_TRUE(isAligned<32>(indirectHeapMemory));
    }

    void TearDown() override {
        alignedFree(indirectHeapMemory);
        DeviceFixture::TearDown();
    }

    const std::array<uint8_t, 3> workgroupWalkOrder = {{0, 1, 2}};
    uint32_t simd;
    uint32_t numChannels;
    uint32_t kernelIsa[32];
    uint8_t *indirectHeapMemory;
    size_t indirectHeapMemorySize;

    SKernelBinaryHeaderCommon kernelHeader;
    SPatchThreadPayload threadPayload;
    SPatchExecutionEnvironment executionEnvironment;
    KernelInfo kernelInfo;
};

typedef PerThreadDataTests<> PerThreadDataXYZTests;

HWTEST_F(PerThreadDataXYZTests, getLocalIdSizePerThread) {
    EXPECT_EQ(3 * 2 * sizeof(GRF), PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels));
}

HWTEST_F(PerThreadDataXYZTests, getPerThreadDataSizeTotal) {
    size_t localWorkSize = 256;
    EXPECT_EQ(256 * 3 * 2 * sizeof(GRF) / 32, PerThreadDataHelper::getPerThreadDataSizeTotal(simd, numChannels, localWorkSize));
}

HWTEST_F(PerThreadDataXYZTests, sendPerThreadData_256x1x1) {
    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    size_t localWorkSizes[3] = {256, 1, 1};
    auto localWorkSize = localWorkSizes[0] * localWorkSizes[1] * localWorkSizes[2];

    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);
    auto expectedPerThreadDataSizeTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(simd, numChannels, localWorkSize);

    size_t sizeConsumed = indirectHeap.getUsed() - offsetPerThreadData;
    EXPECT_EQ(expectedPerThreadDataSizeTotal, sizeConsumed);
}

HWTEST_F(PerThreadDataXYZTests, sendPerThreadData_2x4x8) {
    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    const size_t localWorkSizes[3]{2, 4, 8};
    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
        numChannels,
        localWorkSizes,
        workgroupWalkOrder,
        false);

    size_t sizeConsumed = indirectHeap.getUsed() - offsetPerThreadData;
    EXPECT_EQ(64u * (3u * 2u * 4u * 8u) / 32u, sizeConsumed);
}

HWTEST_F(PerThreadDataXYZTests, getThreadPayloadSize) {
    simd = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 2u * 3u, size);

    simd = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 3u, size);

    simd = 16;
    threadPayload.HeaderPresent = 1;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 4u, size);

    simd = 16;
    threadPayload.UnusedPerThreadConstantPresent = 1;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 5u, size);
}

typedef PerThreadDataTests<false, false, false, false> PerThreadDataNoIdsTests;

HWTEST_F(PerThreadDataNoIdsTests, givenZeroChannelsWhenPassedTogetLocalIdSizePerThreadThenSizeOfOneGrfIsReturned) {
    EXPECT_EQ(32u, PerThreadDataHelper::getLocalIdSizePerThread(simd, numChannels));
}

HWTEST_F(PerThreadDataNoIdsTests, givenZeroChannelsAndHighWkgSizeWhengetPerThreadDataSizeTotalIsCalledThenReturnedSizeContainsUnusedGrfPerEachThread) {
    size_t localWorkSize = 256u;
    auto threadCount = localWorkSize / simd;
    auto expectedSize = threadCount * sizeof(GRF);
    EXPECT_EQ(expectedSize, PerThreadDataHelper::getPerThreadDataSizeTotal(simd, numChannels, localWorkSize));
}

HWTEST_F(PerThreadDataNoIdsTests, sendPerThreadDataDoesntSendAnyData) {
    uint8_t fillValue = 0xcc;
    memset(indirectHeapMemory, fillValue, indirectHeapMemorySize);

    MockGraphicsAllocation gfxAllocation(indirectHeapMemory, indirectHeapMemorySize);
    LinearStream indirectHeap(&gfxAllocation);

    size_t localWorkSizes[3] = {256, 1, 1};
    auto offsetPerThreadData = PerThreadDataHelper::sendPerThreadData(
        indirectHeap,
        simd,
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

HWTEST_F(PerThreadDataNoIdsTests, getThreadPayloadSize) {
    simd = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF), size);

    simd = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF), size);

    simd = 16;
    threadPayload.HeaderPresent = 1;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 2u, size);
}

typedef PerThreadDataTests<false, false, false, true> PerThreadDataFlattenedIdsTests;

HWTEST_F(PerThreadDataFlattenedIdsTests, getThreadPayloadSize) {
    simd = 32;
    uint32_t size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 2u, size);

    simd = 16;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF), size);

    simd = 16;
    threadPayload.HeaderPresent = 1;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 2u, size);

    simd = 32;
    threadPayload.HeaderPresent = 1;
    size = PerThreadDataHelper::getThreadPayloadSize(threadPayload, simd);
    EXPECT_EQ(sizeof(GRF) * 3u, size);
}

TEST(PerThreadDataTest, generateLocalIDs) {
    uint32_t simd = 8;
    uint32_t numChannels = 3;
    uint32_t localWorkSize = 24;

    size_t localWorkSizes[3] = {24, 1, 1};

    auto sizePerThreadDataTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(simd, numChannels, localWorkSize);

    auto sizeOverSizedBuffer = sizePerThreadDataTotal * 4;
    auto buffer = reinterpret_cast<char *>(alignedMalloc(sizeOverSizedBuffer, 16));
    memset(buffer, 0, sizeOverSizedBuffer);

    // Setup reference filled with zeros
    auto reference = reinterpret_cast<char *>(alignedMalloc(sizePerThreadDataTotal, 16));
    memset(reference, 0, sizePerThreadDataTotal);

    LinearStream stream(buffer, sizeOverSizedBuffer / 2);
    PerThreadDataHelper::sendPerThreadData(
        stream,
        simd,
        numChannels,
        localWorkSizes,
        {{0, 1, 2}},
        false);

    // Check if buffer overrun happend, only first sizePerThreadDataTotal bytes can be overwriten, following should be same as reference.
    for (auto i = sizePerThreadDataTotal; i < sizeOverSizedBuffer; i += sizePerThreadDataTotal) {
        int result = memcmp(buffer + i, reference, sizePerThreadDataTotal);
        EXPECT_EQ(0, result);
    }

    alignedFree(buffer);
    alignedFree(reference);
}
