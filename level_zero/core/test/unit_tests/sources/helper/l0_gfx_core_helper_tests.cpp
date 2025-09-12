/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include "multitile_matchers.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTest = ::testing::Test;

TEST(L0GfxCoreHelperTest, WhenL0GfxCoreHelperIsCalledWithUnknownGfxCoreThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, L0GfxCoreHelper::create(IGFX_UNKNOWN_CORE));
}

using PlatformsWithWa = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(L0GfxCoreHelperTest, givenResumeWANeededThenTrueIsReturned, PlatformsWithWa) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_TRUE(l0GfxCoreHelper.isResumeWARequired());
}

HWTEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnFalse) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.imageCompressionSupported(*NEO::defaultHwInfo));

    NEO::debugManager.flags.RenderCompressedImagesEnabled.set(1);
    EXPECT_TRUE(l0GfxCoreHelper.imageCompressionSupported(*NEO::defaultHwInfo));

    NEO::debugManager.flags.RenderCompressedImagesEnabled.set(0);
    EXPECT_FALSE(l0GfxCoreHelper.imageCompressionSupported(*NEO::defaultHwInfo));
}

HWTEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForCmdListWaitOnMemDataSizeThenReturnCorrectSize) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint32_t expectedSize = FamilyType::isQwordInOrderCounter ? sizeof(uint64_t) : sizeof(uint32_t);

    EXPECT_EQ(expectedSize, l0GfxCoreHelper.getCmdListWaitOnMemoryDataSize());
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForUsmCompressionSupportThenReturnFalse, IsAtMostXeCore) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.forceDefaultUsmCompressionSupport());

    HardwareInfo hwInfo = *NEO::defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    EXPECT_FALSE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_FALSE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    EXPECT_TRUE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    EXPECT_FALSE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));
}

HWTEST2_F(L0GfxCoreHelperTest, givenSliceSubsliceEuAndThreadIdsWhenGettingBitmaskThenCorrectBitmaskIsReturned, IsAtMostXe2HpgCore) {

    auto printAttentionBitmask = [](uint8_t *expected, uint8_t *actual, uint32_t maxSlices, uint32_t maxSubSlicesPerSlice, uint32_t maxEuPerSubslice, uint32_t threadsPerEu, bool printBitmask = false) {
        auto bytesPerThread = threadsPerEu > 8 ? 2u : 1u;

        auto maxEUsInAtt = maxEuPerSubslice > 8 ? 8 : maxEuPerSubslice;
        auto bytesPerSlice = maxSubSlicesPerSlice * maxEUsInAtt * bytesPerThread;
        auto bytesPerSubSlice = maxEUsInAtt * bytesPerThread;

        for (uint32_t slice = 0; slice < maxSlices; slice++) {
            for (uint32_t subslice = 0; subslice < maxSubSlicesPerSlice; subslice++) {
                for (uint32_t eu = 0; eu < maxEUsInAtt; eu++) {
                    for (uint32_t byte = 0; byte < bytesPerThread; byte++) {
                        if (printBitmask) {
                            std::bitset<8> bits(actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                            std::cout << " slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bits << "\n";
                        }

                        if (expected[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte] !=
                            actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]) {
                            std::bitset<8> bits(actual[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                            std::bitset<8> bitsExpected(expected[slice * bytesPerSlice + subslice * bytesPerSubSlice + eu * bytesPerThread + byte]);
                            ASSERT_FALSE(true) << " got: slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bits << "\n"
                                               << " expected: slice = " << slice << " subslice = " << subslice << " eu = " << eu << " threads bitmask = " << bitsExpected << "\n";
                        }
                    }
                }
            }
        }

        if (printBitmask) {
            std::cout << "\n\n";
        }
    };

    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 16u;
    hwInfo.gtSystemInfo.EUCount = hwInfo.gtSystemInfo.MaxEuPerSubSlice * hwInfo.gtSystemInfo.SubSliceCount;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;
    MockExecutionEnvironment executionEnvironment(&hwInfo);
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subslice = subslicesPerSlice > 1 ? subslicesPerSlice - 1 : 0;

    auto bytesPerEu = Math::divideAndRoundUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8u);

    const auto maxEUsInAtt = hwInfo.gtSystemInfo.MaxEuPerSubSlice > 8 ? 8 : hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const auto threadsSizePerSubSlice = maxEUsInAtt * bytesPerEu;
    const auto threadsSizePerSlice = threadsSizePerSubSlice * subslicesPerSlice;

    std::vector<EuThread::ThreadId> threads;
    threads.push_back({0, 0, 0, 0, 6});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    auto expectedBitmask = std::make_unique<uint8_t[]>(size);
    uint8_t *data = nullptr;
    memset(expectedBitmask.get(), 0, size);

    auto returnedBitmask = bitmask.get();
    EXPECT_EQ(uint8_t(1u << 6), returnedBitmask[0]);

    threads.clear();
    threads.push_back({0, 0, 0, 1, 3});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);
    returnedBitmask = bitmask.get();
    returnedBitmask += bytesPerEu;
    EXPECT_EQ(uint8_t(1u << 3), returnedBitmask[0]);

    threads.clear();
    threads.push_back({0, 0, subslice, 3, 6});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, subslice * threadsSizePerSubSlice);
    data = ptrOffset(data, 3 * bytesPerEu);
    data[0] = 1 << 6;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, hwInfo.gtSystemInfo.NumThreadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    threads.clear();
    threads.push_back({0, hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subslice, 3, 6});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);
    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subslice * threadsSizePerSubSlice);
    data = ptrOffset(data, 3 * bytesPerEu);
    data[0] = 1 << 6;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, hwInfo.gtSystemInfo.NumThreadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));

    threads.clear();
    threads.push_back({0, hwInfo.gtSystemInfo.MaxSlicesSupported - 1, subslice, maxEUsInAtt - 1, 0});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);
    data = expectedBitmask.get();
    memset(expectedBitmask.get(), 0, size);

    data = ptrOffset(data, (hwInfo.gtSystemInfo.MaxSlicesSupported - 1) * threadsSizePerSlice);
    data = ptrOffset(data, subslice * threadsSizePerSubSlice);

    if (l0GfxCoreHelper.isResumeWARequired()) {
        data = ptrOffset(data, (maxEUsInAtt - 1) % 4 * bytesPerEu);
    } else {
        data = ptrOffset(data, (maxEUsInAtt - 1) * bytesPerEu);
    }
    data[0] = 1;

    printAttentionBitmask(expectedBitmask.get(), bitmask.get(), hwInfo.gtSystemInfo.MaxSlicesSupported, subslicesPerSlice, hwInfo.gtSystemInfo.MaxEuPerSubSlice, hwInfo.gtSystemInfo.NumThreadsPerEu);
    EXPECT_EQ(0, memcmp(bitmask.get(), expectedBitmask.get(), size));
}

HWTEST2_F(L0GfxCoreHelperTest, givenSingleThreadsWhenGettingBitmaskThenCorrectBitsAreSet, IsAtMostXe2HpgCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    std::vector<EuThread::ThreadId> threads;
    threads.push_back({0, 0, 0, 0, 3});
    threads.push_back({0, 0, 0, 1, 0});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    auto numBytesPerThread = Math::divideAndRoundUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8u);

    auto data = bitmask.get();
    EXPECT_EQ(1u << 3, data[0]);
    EXPECT_EQ(1u, data[numBytesPerThread]);

    EXPECT_TRUE(memoryZeroed(&data[numBytesPerThread + 1], size - numBytesPerThread - 1));
}

HWTEST2_F(L0GfxCoreHelperTest, givenSingleThreadsWhenGettingBitmaskThenCorrectBitsAreSet, IsXe3Core) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 4u;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 16u;
    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;

    hwInfo.gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;

    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t bytesPerEu = alignUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    std::vector<EuThread::ThreadId> threads;
    threads.push_back({0, 0, 0, 0, 8});
    threads.push_back({0, 0, 0, 1, 9});

    threads.push_back({0, 1, 0, 0, 0});
    threads.push_back({0, 1, 0, 1, 1});
    threads.push_back({0, 1, 0, 0, 8});
    threads.push_back({0, 1, 0, 1, 9});

    threads.push_back({0, 1, 1, 0, 0});
    threads.push_back({0, 1, 1, 1, 1});
    threads.push_back({0, 1, 1, 0, 8});
    threads.push_back({0, 1, 1, 1, 9});

    threads.push_back({0, 2, 1, 0, 0});
    threads.push_back({0, 2, 1, 1, 1});
    threads.push_back({0, 2, 1, 0, 8});
    threads.push_back({0, 2, 1, 1, 9});

    threads.push_back({0, 1, 2, 0, 0});
    threads.push_back({0, 1, 2, 1, 1});
    threads.push_back({0, 1, 2, 0, 8});
    threads.push_back({0, 1, 2, 1, 9});

    auto maxSlice = hwInfo.gtSystemInfo.MaxSlicesSupported - 1;
    threads.push_back({0, maxSlice, 2, 3, 0});
    threads.push_back({0, maxSlice, 2, 3, 1});
    threads.push_back({0, maxSlice, 2, 3, 8});
    threads.push_back({0, maxSlice, 2, 3, 9});

    auto maxSubSlice = numSubslicesPerSlice - 1;
    threads.push_back({0, 1, maxSubSlice, 3, 0});
    threads.push_back({0, 1, maxSubSlice, 3, 1});
    threads.push_back({0, 1, maxSubSlice, 3, 8});
    threads.push_back({0, 1, maxSubSlice, 3, 9});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    auto data = bitmask.get();
    EXPECT_EQ(1u, data[8]);
    EXPECT_EQ(1u << 1, data[9]);

    auto sliceOffset = threadsSizePerSlice;
    EXPECT_EQ(1u, data[sliceOffset]);
    EXPECT_EQ(1u << 1, data[sliceOffset + 1]);

    EXPECT_EQ(1u, data[sliceOffset + 8]);
    EXPECT_EQ(1u << 1, data[sliceOffset + 9]);

    auto subSliceOffset = sliceOffset + numEuPerSubslice * bytesPerEu;
    EXPECT_EQ(1u, data[subSliceOffset]);
    EXPECT_EQ(1u << 1, data[subSliceOffset + 1]);

    EXPECT_EQ(1u, data[subSliceOffset + 8]);
    EXPECT_EQ(1u << 1, data[subSliceOffset + 9]);

    size_t threadCount = 0;
    for (size_t i = 0; i < size; i++) {
        while (data[i]) {
            if (data[i] & 0x01) {
                threadCount++;
            }
            data[i] = data[i] >> 1;
        }
    }
    EXPECT_EQ(threadCount, threads.size());
}

HWTEST2_F(L0GfxCoreHelperTest, givenSliceSubsliceEuAndThreadIdsWhenGettingBitmaskThenCorrectBitmaskIsReturned, IsXe3Core) {

    auto hwInfo = *NEO::defaultHwInfo.get();

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 16u;
    hwInfo.gtSystemInfo.EUCount = hwInfo.gtSystemInfo.MaxEuPerSubSlice * hwInfo.gtSystemInfo.SubSliceCount;
    hwInfo.gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;
    MockExecutionEnvironment executionEnvironment(&hwInfo);
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t bytesPerEu = alignUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    auto sliceOffset = threadsSizePerSlice;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    std::vector<EuThread::ThreadId> threads;
    threads.push_back({0, 0, 0, 0, 6});
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    auto expectedBitmask = std::make_unique<uint8_t[]>(size);
    memset(expectedBitmask.get(), 0, size);

    auto returnedBitmask = bitmask.get();
    EXPECT_EQ(uint8_t(1u << 6), returnedBitmask[0]);

    threads.clear();
    threads.push_back({0, 0, 0, 1, 3});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    returnedBitmask = bitmask.get();
    EXPECT_EQ(uint8_t(1u << 3), returnedBitmask[1]);

    threads.clear();
    threads.push_back({0, 0, 1, 1, 8});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    returnedBitmask = bitmask.get();
    EXPECT_EQ(1u, returnedBitmask[25]);

    threads.clear();
    threads.push_back({0, 1, 0, 0, 8});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, size);

    returnedBitmask = bitmask.get();
    EXPECT_EQ(1u, returnedBitmask[sliceOffset + 8]);
}

HWTEST_F(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subsliceID = subslicesPerSlice > 2 ? subslicesPerSlice - 2 : 0;

    uint32_t threadID = 3;
    std::vector<EuThread::ThreadId> threadsWithAtt;
    threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(1u, threads.size());
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subsliceID, threads[0].subslice);
    EXPECT_EQ(0u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);

    EXPECT_EQ(0u, threads[0].tileIndex);

    threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 1, bitmask.get(), size);

    ASSERT_EQ(1u, threads.size());
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subsliceID, threads[0].subslice);
    EXPECT_EQ(0u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);

    EXPECT_EQ(1u, threads[0].tileIndex);
}

HWTEST2_F(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IsXe3Core) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    hwInfo.gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subsliceID = subslicesPerSlice > 2 ? subslicesPerSlice - 2 : 0;

    uint32_t threadID = 8;
    std::vector<EuThread::ThreadId> threadsWithAtt;
    threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(1u, threads.size());
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subsliceID, threads[0].subslice);
    EXPECT_EQ(0u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);

    EXPECT_EQ(0u, threads[0].tileIndex);

    std::memset(bitmask.get(), 0, size);
    threadsWithAtt.clear();
    threadID = 9;
    threadsWithAtt.push_back({0, 0, 1, 5, threadID});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);
    threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(1u, threads.size());
    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(1u, threads[0].subslice);
    EXPECT_EQ(5u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);
}

HWTEST_F(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 0;

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t subsliceID = 0; subsliceID < subslicesPerSlice; subsliceID++) {
        threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(subslicesPerSlice, threads.size());

    for (uint32_t i = 0; i < subslicesPerSlice; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(i, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);

        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

HWTEST_F(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    const auto numEUsPerSS = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    uint32_t threadID = 3;

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t euId = 0; euId < numEUsPerSS; euId++) {
        threadsWithAtt.push_back({0, 0, 0, euId, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);
    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(numEUsPerSS, threads.size());

    for (uint32_t i = 0; i < numEUsPerSS; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(0u, threads[i].subslice);
        EXPECT_EQ(i, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);

        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

HWTEST2_F(L0GfxCoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IsWithinXeHpcCoreAndXe2HpgCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    auto numBytesPerEu = Math::divideAndRoundUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8u);

    uint8_t data[4]{};
    data[0] = 0x0f;
    data[numBytesPerEu] = 0x0f;
    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, data, sizeof(data));

    ASSERT_EQ(8u, threads.size());
    ze_device_thread_t expectedThreads[] = {
        {0, 0, 0, 0},
        {0, 0, 0, 1},
        {0, 0, 0, 2},
        {0, 0, 0, 3},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 1, 2},
        {0, 0, 1, 3}};

    for (uint32_t i = 0; i < 8u; i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);

        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

HWTEST2_F(L0GfxCoreHelperTest, givenEu0To1Threads6To10BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IsXe3Core) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    hwInfo.gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;

    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t bytesPerEu = alignUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;

    auto subsliceOffset = numEuPerSubslice * bytesPerEu;
    auto sliceOffset = threadsSizePerSlice;

    uint8_t data[1024] = {};
    data[0] = 0xC0;
    data[1] = 0xC0;
    data[8] = 0x03;
    data[9] = 0x03;
    data[subsliceOffset + 8] = 0x03;
    data[subsliceOffset + 9] = 0x03;
    data[sliceOffset + subsliceOffset + 8] = 0x03;
    data[sliceOffset + subsliceOffset + 9] = 0x03;

    ze_device_thread_t expectedThreads[] = {
        {0, 0, 0, 6},
        {0, 0, 0, 7},
        {0, 0, 1, 6},
        {0, 0, 1, 7},
        {0, 0, 0, 8},
        {0, 0, 0, 9},
        {0, 0, 1, 8},
        {0, 0, 1, 9},
        // subslice > 0
        {0, 1, 0, 8},
        {0, 1, 0, 9},
        {0, 1, 1, 8},
        {0, 1, 1, 9},
        // slice > 0
        {1, 1, 0, 8},
        {1, 1, 0, 9},
        {1, 1, 1, 8},
        {1, 1, 1, 9}};

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, data, sizeof(data));
    ASSERT_EQ(16u, threads.size());

    for (uint32_t i = 0; i < 16u; i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);
        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

HWTEST2_F(L0GfxCoreHelperTest, givenThreadsToBitmaskThenSameThreadsReturnedParsingBitmask, IsXe3Core) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 4u;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 16u;
    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;

    hwInfo.gtSystemInfo.NumThreadsPerEu = 10u;
    hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    // ordering is important, byte0 of every EU is before byte1 of any EU
    std::vector<EuThread::ThreadId> expectedThreads = {
        {0, 0, 0, 0, 6},
        {0, 0, 0, 0, 7},
        {0, 0, 0, 1, 6},
        {0, 0, 0, 1, 7},
        {0, 0, 0, 0, 8},
        {0, 0, 0, 0, 9},
        {0, 0, 0, 1, 8},
        {0, 0, 0, 1, 9},
        {0, 0, 1, 0, 8},
        {0, 0, 1, 0, 9},
        {0, 0, 1, 1, 8},
        {0, 0, 1, 1, 9},
        {0, 1, 1, 3, 5},
        {0, 1, 1, 6, 7},
        {0, 1, 1, 0, 8},
        {0, 1, 1, 0, 9},
        {0, 1, 1, 1, 8},
        {0, 1, 1, 1, 9},
        {0, 1, 1, 2, 8},
        {0, 1, 1, 4, 9},
        {0, 2, 1, 0, 0},
        {0, 2, 2, 3, 5}};

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(expectedThreads, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    for (uint32_t i = 0; i < expectedThreads.size(); i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);

        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

HWTEST_F(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 3;
    auto numOfActiveSubslices = ((subslicesPerSlice + 1) / 2);

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t subsliceID = 0; subsliceID < numOfActiveSubslices; subsliceID++) {
        threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto bitmaskSizePerSingleSubslice = size / hwInfo.gtSystemInfo.MaxSlicesSupported / subslicesPerSlice;

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), bitmaskSizePerSingleSubslice * numOfActiveSubslices);

    ASSERT_EQ(numOfActiveSubslices, threads.size());

    for (uint32_t i = 0; i < numOfActiveSubslices; i++) {
        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(i, threads[i].subslice);
        EXPECT_EQ(0u, threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);

        EXPECT_EQ(0u, threads[i].tileIndex);
    }
}

using PlatformsWithFusedEus = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HPG_CORE>;
using L0GfxCoreHelperFusedEuTest = L0GfxCoreHelperTest;

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenBitmaskWithAttentionBitsWith8EUSSWhenGettingThreadsThenSingleCorrectThreadReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 8;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 32;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    for (uint32_t subsliceID = 0; subsliceID < 2; subsliceID++) {

        std::vector<EuThread::ThreadId> threadsWithAtt;
        threadsWithAtt.push_back({0, 0, subsliceID, 0, 0});

        l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

        auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

        ASSERT_EQ(2u, threads.size());

        EXPECT_EQ(0u, threads[0].slice);
        EXPECT_EQ(subsliceID, threads[0].subslice);
        EXPECT_EQ(0u, threads[0].eu);
        EXPECT_EQ(0u, threads[0].thread);
        EXPECT_EQ(0u, threads[0].tileIndex);

        EXPECT_EQ(0u, threads[1].slice);
        EXPECT_EQ(subsliceID, threads[1].subslice);
        EXPECT_EQ(4u, threads[1].eu);
        EXPECT_EQ(0u, threads[1].thread);
        EXPECT_EQ(0u, threads[1].tileIndex);
    }
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenDynamicallyPopulatesSliceInfoGreaterThanMaxSlicesSupportedThenBitmasksAreCorrect, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 2;
    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;

    std::vector<EuThread::ThreadId> threadsWithAtt;
    threadsWithAtt.push_back({0, 2, 0, 0, 0});
    threadsWithAtt.push_back({0, 3, 0, 0, 0});
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = std::min(hwInfo.gtSystemInfo.MaxEuPerSubSlice, 8u);
    const uint32_t bytesPerEu = alignUp(hwInfo.gtSystemInfo.NumThreadsPerEu, 8) / 8;
    auto expectedSize = 4 * numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    EXPECT_EQ(size, expectedSize);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);
    ASSERT_EQ(threads.size(), 4u);
    EXPECT_EQ(threads[0], threadsWithAtt[0]);
    EXPECT_EQ(threads[2], threadsWithAtt[1]);
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenThreadForTwoEUsReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t subsliceID = subslicesPerSlice > 2 ? subslicesPerSlice - 2 : 0;

    uint32_t threadID = 3;
    std::vector<EuThread::ThreadId> threadsWithAtt;
    threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(2u, threads.size());

    EXPECT_EQ(0u, threads[0].slice);
    EXPECT_EQ(subsliceID, threads[0].subslice);
    EXPECT_EQ(0u, threads[0].eu);
    EXPECT_EQ(threadID, threads[0].thread);
    EXPECT_EQ(0u, threads[0].tileIndex);

    EXPECT_EQ(0u, threads[1].slice);
    EXPECT_EQ(subsliceID, threads[1].subslice);
    EXPECT_EQ(4u, threads[1].eu);
    EXPECT_EQ(threadID, threads[1].thread);
    EXPECT_EQ(0u, threads[1].tileIndex);
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsForTwoEUsAreReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 0;

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t subsliceID = 0; subsliceID < subslicesPerSlice; subsliceID++) {
        threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(2 * subslicesPerSlice, threads.size());

    auto threadIndex = 0;
    for (uint32_t i = 0; i < subslicesPerSlice; i++) {
        EXPECT_EQ(0u, threads[threadIndex].slice);
        EXPECT_EQ(i, threads[threadIndex].subslice);
        EXPECT_EQ(threadID, threads[threadIndex].thread);
        EXPECT_EQ(0u, threads[threadIndex].eu);

        threadIndex++;
        EXPECT_EQ(threadID, threads[threadIndex].thread);
        EXPECT_EQ(4u, threads[threadIndex].eu);
        threadIndex++;
    }
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    const auto maxEUsInAtt = 8u;
    uint32_t threadID = 3;

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t euId = 0; euId < maxEUsInAtt; euId++) {
        threadsWithAtt.push_back({0, 0, 0, euId, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);
    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), size);

    ASSERT_EQ(maxEUsInAtt, threads.size());

    uint32_t expectedEUs[] = {0, 4, 1, 5, 2, 6, 3, 7};
    for (uint32_t i = 0; i < threads.size(); i++) {

        EXPECT_EQ(0u, threads[i].slice);
        EXPECT_EQ(0u, threads[i].subslice);
        EXPECT_EQ(expectedEUs[i], threads[i].eu);
        EXPECT_EQ(threadID, threads[i].thread);
    }
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint8_t data[2] = {0x0f, 0x0f};
    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, data, sizeof(data));

    ASSERT_EQ(16u, threads.size());

    ze_device_thread_t expectedThreads[] = {
        {0, 0, 0, 0}, {0, 0, 4, 0}, {0, 0, 0, 1}, {0, 0, 4, 1}, {0, 0, 0, 2}, {0, 0, 4, 2}, {0, 0, 0, 3}, {0, 0, 4, 3}, {0, 0, 1, 0}, {0, 0, 5, 0}, {0, 0, 1, 1}, {0, 0, 5, 1}, {0, 0, 1, 2}, {0, 0, 5, 2}, {0, 0, 1, 3}, {0, 0, 5, 3}};

    for (uint32_t i = 0; i < 16u; i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);
    }
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenEu8To9Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f};
    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, data, sizeof(data));

    ASSERT_EQ(16u, threads.size());

    ze_device_thread_t expectedThreads[] = {
        {0, 0, 8, 0}, {0, 0, 12, 0}, {0, 0, 8, 1}, {0, 0, 12, 1}, {0, 0, 8, 2}, {0, 0, 12, 2}, {0, 0, 8, 3}, {0, 0, 12, 3}, {0, 0, 9, 0}, {0, 0, 13, 0}, {0, 0, 9, 1}, {0, 0, 13, 1}, {0, 0, 9, 2}, {0, 0, 13, 2}, {0, 0, 9, 3}, {0, 0, 13, 3}};

    for (uint32_t i = 0; i < 16u; i++) {
        EXPECT_EQ(expectedThreads[i].slice, threads[i].slice);
        EXPECT_EQ(expectedThreads[i].subslice, threads[i].subslice);
        EXPECT_EQ(expectedThreads[i].eu, threads[i].eu);
        EXPECT_EQ(expectedThreads[i].thread, threads[i].thread);
    }
}

HWTEST2_F(L0GfxCoreHelperFusedEuTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, PlatformsWithFusedEus) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    if (hwInfo.gtSystemInfo.MaxEuPerSubSlice <= 8) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    std::unique_ptr<uint8_t[]> bitmask;
    size_t size = 0;

    uint32_t subslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    uint32_t threadID = 3;
    auto numOfActiveSubslices = ((subslicesPerSlice + 1) / 2);

    std::vector<EuThread::ThreadId> threadsWithAtt;
    for (uint32_t subsliceID = 0; subsliceID < numOfActiveSubslices; subsliceID++) {
        threadsWithAtt.push_back({0, 0, subsliceID, 0, threadID});
    }

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threadsWithAtt, hwInfo, bitmask, size);

    auto bitmaskSizePerSingleSubslice = size / hwInfo.gtSystemInfo.MaxSlicesSupported / subslicesPerSlice;

    auto threads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0, bitmask.get(), bitmaskSizePerSingleSubslice * numOfActiveSubslices);

    ASSERT_EQ(2 * numOfActiveSubslices, threads.size());

    uint32_t subsliceIndex = 0;
    for (uint32_t i = 0; i < threads.size(); i++) {

        if (i % 2 == 0) {
            EXPECT_EQ(0u, threads[i].slice);
            EXPECT_EQ(subsliceIndex, threads[i].subslice);
            EXPECT_EQ(0u, threads[i].eu);
            EXPECT_EQ(threadID, threads[i].thread);

        } else {
            EXPECT_EQ(0u, threads[i].slice);
            EXPECT_EQ(subsliceIndex, threads[i].subslice);
            EXPECT_EQ(4u, threads[i].eu);
            EXPECT_EQ(threadID, threads[i].thread);

            subsliceIndex++;
        }
    }
}

HWTEST2_F(L0GfxCoreHelperTest, whenAlwaysAllocateEventInLocalMemCalledThenReturnFalse, IsNotXeHpcCore) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.alwaysAllocateEventInLocalMem());
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultValueForUsePipeControlMultiKernelEventSyncThenReturnTrue) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    bool defaultValue = L0::L0GfxCoreHelper::usePipeControlMultiKernelEventSync(hwInfo);
    EXPECT_TRUE(defaultValue);
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultValueForCompactL3FlushEventPacketThenReturnTrue) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    bool useL3FlushAfterPostSync = false;
    bool defaultValue = L0::L0GfxCoreHelper::useCompactL3FlushEventPacket(hwInfo, useL3FlushAfterPostSync);
    EXPECT_TRUE(defaultValue);
}

TEST_F(L0GfxCoreHelperTest, givenL3FlushAfterPostSyncWhenUseCompactL3FlushEventPacketThenFalseIsReturned) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    bool useL3FlushAfterPostSync = true;
    bool compactL3FlushEventEnabled = L0::L0GfxCoreHelper::useCompactL3FlushEventPacket(hwInfo, useL3FlushAfterPostSync);
    EXPECT_FALSE(compactL3FlushEventEnabled);
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultValueForDynamicEventPacketCountThenReturnTrue) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    bool defaultValue = L0::L0GfxCoreHelper::useDynamicEventPacketsCount(hwInfo);
    EXPECT_TRUE(defaultValue);
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingMaxKernelAndMaxPacketThenExpectBothReturnOne, MultiTileNotSupported) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(1u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(1u, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

template <int32_t usePipeControlMultiPacketEventSync, int32_t compactL3FlushEventPacket>
struct L0GfxCoreHelperMultiPacketEventFixture {
    void setUp() {
        debugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        debugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    }

    void tearDown() {
    }

    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
};

using L0GfxCoreHelperEventMultiKernelEnabledL3FlushCompactDisabledTest = Test<L0GfxCoreHelperMultiPacketEventFixture<0, 0>>;
HWTEST2_F(L0GfxCoreHelperEventMultiKernelEnabledL3FlushCompactDisabledTest,
          givenL0GfxCoreHelperWhenGettingMaxKernelAndMaxPacketThenExpectKernelThreeAndPacketThreeWithL3PacketWhenApplicable,
          IsAtLeastXeCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint32_t expectedPacket = 3;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *executionEnvironment.rootDeviceEnvironments[0])) {
        expectedPacket++;
    }

    EXPECT_EQ(3u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

using L0GfxCoreHelperEventMultiKernelEnabledL3FlushCompactEnabledTest = Test<L0GfxCoreHelperMultiPacketEventFixture<0, 1>>;
HWTEST2_F(L0GfxCoreHelperEventMultiKernelEnabledL3FlushCompactEnabledTest,
          givenL0GfxCoreHelperWhenGettingMaxKernelAndMaxPacketThenExpectKernelThreeAndPacketThree,
          IsAtLeastXeCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint32_t expectedPacket = 3;

    EXPECT_EQ(3u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

using L0GfxCoreHelperEventMultiKernelDisabledL3FlushCompactDisabledTest = Test<L0GfxCoreHelperMultiPacketEventFixture<1, 0>>;
HWTEST2_F(L0GfxCoreHelperEventMultiKernelDisabledL3FlushCompactDisabledTest,
          givenL0GfxCoreHelperWhenGettingMaxKernelAndMaxPacketThenExpectKernelOneAndPacketOneWithL3PacketWhenApplicable,
          IsAtLeastXeCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint32_t expectedPacket = 1;
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *executionEnvironment.rootDeviceEnvironments[0])) {
        expectedPacket++;
    }

    EXPECT_EQ(1u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

using L0GfxCoreHelperEventMultiKernelDisabledL3FlushCompactEnabledTest = Test<L0GfxCoreHelperMultiPacketEventFixture<1, 1>>;
HWTEST2_F(L0GfxCoreHelperEventMultiKernelDisabledL3FlushCompactEnabledTest,
          givenL0GfxCoreHelperWhenGettingMaxKernelAndMaxPacketThenExpectKernelOneAndPacketOne,
          IsAtLeastXeCore) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    uint32_t expectedPacket = 1;

    EXPECT_EQ(1u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultValueForSignalAllEventPacketThenReturnTrue) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    bool defaultValue = L0::L0GfxCoreHelper::useSignalAllEventPackets(hwInfo);
    EXPECT_TRUE(defaultValue);
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultHeapModelThenUsePlatformDefaultHeapModel) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(l0GfxCoreHelper.getPlatformHeapAddressModel(rootDeviceEnvironment), L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingDefaultHeapModelThenUseDebugKeySelectedModel) {
    DebugManagerStateRestore restorer;

    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment));

    debugManager.flags.SelectCmdListHeapAddressModel.set(1);

    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment));

    debugManager.flags.SelectCmdListHeapAddressModel.set(2);

    EXPECT_EQ(NEO::HeapAddressModel::globalBindless, L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment));

    debugManager.flags.SelectCmdListHeapAddressModel.set(3);

    EXPECT_EQ(NEO::HeapAddressModel::globalBindful, L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingDispatchCmdListCmdBufferPrimaryThenUseDbgKeyValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    EXPECT_FALSE(L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, true));

    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(1);

    EXPECT_TRUE(L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, true));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingDispatchCmdListCmdBufferPrimaryAndNotAllowPrimaryThenOverrideDbgKeyValueAndDisallow) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(1);

    EXPECT_FALSE(L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, false));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultCmdlistPrimaryBatchBufferThenUsePlatformDefaultSetting) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList(), L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, true));
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperOnGenPlatformsWhenGettingPlatformUseImmediateFlushTaskThenReturnFalse, IsGen12LP) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperOnGenPlatformsWhenGettingCmdlistUpdateCapabilityThenReturnZero, IsGen12LP) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperOnGenPlatformsWhenGettingRecordReplayGraphCapabilityThenReturnZero, IsGen12LP) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities());
}

HWTEST_F(L0GfxCoreHelperTest, whenAskingForUnifiedPostSyncAllocLayoutThenCheckImmWriteOffset) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_EQ((NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset() == NEO::ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset()),
              l0GfxCoreHelper.hasUnifiedPostSyncAllocationLayout());
}

HWTEST_F(L0GfxCoreHelperTest, whenAskingForImmediateWritePostSyncOffsetThenReturnValueFromImplicitScalingHelper) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), l0GfxCoreHelper.getImmediateWritePostSyncOffset());
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultUseImmediateFlushTaskThenUsePlatformDefaultSetting) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask(), L0GfxCoreHelper::useImmediateComputeFlushTask(rootDeviceEnvironment));
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingSyncDispatchSupportThenReturnFalse, IsAtMostXeHpgCore) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.synchronizedDispatchSupported());
}

HWTEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingImplicitSyncDispatchModeForCooperativeKernelsThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.implicitSynchronizedDispatchForCooperativeKernelsAllowed());
}

HWTEST2_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingSyncDispatchSupportThenReturnTrue, IsAtLeastXeHpcCore) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_TRUE(l0GfxCoreHelper.synchronizedDispatchSupported());
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingUseImmediateFlushTaskThenUseDbgKeyValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    debugManager.flags.UseImmediateFlushTask.set(0);

    EXPECT_FALSE(L0GfxCoreHelper::useImmediateComputeFlushTask(rootDeviceEnvironment));

    debugManager.flags.UseImmediateFlushTask.set(1);

    EXPECT_TRUE(L0GfxCoreHelper::useImmediateComputeFlushTask(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingCmdListUpdateCapabilityThenUseDbgKeyValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    constexpr uint32_t expectedValue = 2359;
    debugManager.flags.OverrideCmdListUpdateCapability.set(static_cast<int32_t>(expectedValue));

    EXPECT_EQ(expectedValue, L0GfxCoreHelper::getCmdListUpdateCapabilities(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultCmdlistUpdateCapabilityThenUsePlatformDefaultSetting) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities(), L0GfxCoreHelper::getCmdListUpdateCapabilities(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperUsingOverrideDebugKeyWhenGettingRecordReplayGraphCapabilityThenUseDbgKeyValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();

    constexpr uint32_t expectedValue = 2359;
    debugManager.flags.OverrideRecordReplayGraphCapability.set(static_cast<int32_t>(expectedValue));

    EXPECT_EQ(expectedValue, L0GfxCoreHelper::getRecordReplayGraphCapabilities(rootDeviceEnvironment));
}

TEST_F(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingDefaultRecordReplayGraphCapabilityThenUsePlatformDefaultSetting) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_EQ(l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities(), L0GfxCoreHelper::getRecordReplayGraphCapabilities(rootDeviceEnvironment));
}

} // namespace ult
} // namespace L0
