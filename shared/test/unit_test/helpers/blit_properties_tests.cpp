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
        nullptr);
    EXPECT_EQ(0x2000u, props.srcGpuAddress);
    EXPECT_EQ(0x1000u, props.dstGpuAddress);
}
