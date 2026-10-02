/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(BlitSyncPropertiesTest, givenTimestampSyncModesWhenIsTimestampModeCalledThenReturnsTrue) {
    BlitSyncProperties props{};
    for (auto mode : {BlitSyncMode::timestamp, BlitSyncMode::timestampAndImmediate}) {
        props.syncMode = mode;
        EXPECT_TRUE(props.isTimestampMode());
    }
}

TEST(BlitSyncPropertiesTest, givenNonTimestampSyncModesWhenIsTimestampModeCalledThenReturnsFalse) {
    BlitSyncProperties props{};
    for (auto mode : {BlitSyncMode::none, BlitSyncMode::immediate}) {
        props.syncMode = mode;
        EXPECT_FALSE(props.isTimestampMode());
    }
}

TEST(BlitPropertiesTest, givenBlitParamsToConstructWhenSrcAndDstPtrPassedThenAllocationBaseAddressNotUsed) {
    NEO::MockGraphicsAllocation mockSrcGa;
    mockSrcGa.setGpuBaseAddress(0x1234);
    NEO::MockGraphicsAllocation mockDstGa;
    mockDstGa.setGpuBaseAddress(0x5678);
    auto props = BlitProperties::constructPropertiesForCopy(
        &mockDstGa, 0x1000,
        &mockSrcGa, 0x2000,
        {0, 0, 0}, {0, 0, 0}, {64, 64, 1},
        64, 4096,
        64, 4096,
        nullptr, false);
    EXPECT_EQ(0x2000u, props.srcGpuAddress);
    EXPECT_EQ(0x1000u, props.dstGpuAddress);
}

TEST(BlitPropertiesTest, givenDstInLocalMemoryAndNotRemoteWhenConstructingPropertiesForCopyThenDstIsNotSystemOrRemoteMemory) {
    NEO::MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(MemoryPool::system4KBPages);
    NEO::MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(MemoryPool::localMemory);

    auto props = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0x1000, &srcAlloc, 0x2000,
                                                            {0, 0, 0}, {0, 0, 0}, {64, 1, 1}, 0, 0, 0, 0, nullptr, false);
    EXPECT_FALSE(props.isDstSystemOrRemoteMemory);
}

TEST(BlitPropertiesTest, givenDstInLocalMemoryAndRemoteWhenConstructingPropertiesForCopyThenDstIsSystemOrRemoteMemory) {
    NEO::MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(MemoryPool::localMemory);
    NEO::MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(MemoryPool::localMemory);

    auto props = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0x1000, &srcAlloc, 0x2000,
                                                            {0, 0, 0}, {0, 0, 0}, {64, 1, 1}, 0, 0, 0, 0, nullptr, true);
    EXPECT_TRUE(props.isDstSystemOrRemoteMemory);
}

TEST(BlitPropertiesTest, givenDstInSystemMemoryWhenConstructingPropertiesForCopyThenDstIsSystemOrRemoteMemory) {
    NEO::MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(MemoryPool::localMemory);
    for (auto pool : {MemoryPool::system4KBPages, MemoryPool::system64KBPages, MemoryPool::system4KBPagesWith32BitGpuAddressing, MemoryPool::system64KBPagesWith32BitGpuAddressing}) {
        NEO::MockGraphicsAllocation dstAlloc;
        dstAlloc.overrideMemoryPool(pool);

        auto props = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0x1000, &srcAlloc, 0x2000,
                                                                {0, 0, 0}, {0, 0, 0}, {64, 1, 1}, 0, 0, 0, 0, nullptr, false);
        EXPECT_TRUE(props.isDstSystemOrRemoteMemory);
        EXPECT_FALSE(props.isSystemMemoryPoolUsed);
    }
}

TEST(BlitPropertiesTest, givenNoDstAllocationWhenConstructingPropertiesForCopyThenDstIsSystemOrRemoteMemory) {
    NEO::MockGraphicsAllocation srcAlloc;
    srcAlloc.overrideMemoryPool(MemoryPool::localMemory);

    auto props = BlitProperties::constructPropertiesForCopy(nullptr, 0x1000, &srcAlloc, 0x2000,
                                                            {0, 0, 0}, {0, 0, 0}, {64, 1, 1}, 0, 0, 0, 0, nullptr, false);
    EXPECT_TRUE(props.isDstSystemOrRemoteMemory);
}

TEST(BlitPropertiesTest, givenDstInLocalMemoryAndSrcInSystemMemoryWhenConstructingPropertiesForCopyThenDstIsNotSystemOrRemoteMemory) {
    NEO::MockGraphicsAllocation dstAlloc;
    dstAlloc.overrideMemoryPool(MemoryPool::localMemory);

    auto props = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0x1000, nullptr, 0x2000,
                                                            {0, 0, 0}, {0, 0, 0}, {64, 1, 1}, 0, 0, 0, 0, nullptr, false);
    EXPECT_TRUE(props.isSystemMemoryPoolUsed);
    EXPECT_FALSE(props.isDstSystemOrRemoteMemory);
}
