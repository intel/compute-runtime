/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "test.h"

namespace L0 {
namespace ult {

TEST(CompressionSelectorL0Tests, GivenDefaultDebugFlagWhenProvidingUsmAllocationThenExpectCompressionDisabled) {
    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    GraphicsAllocation::AllocationType::BUFFER,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_FALSE(NEO::CompressionSelector::preferRenderCompressedBuffer(properties));
}

TEST(CompressionSelectorL0Tests, GivenDisabledDebugFlagWhenProvidingUsmAllocationThenExpectCompressionDisabled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableUsmCompression.set(0);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    GraphicsAllocation::AllocationType::BUFFER,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_FALSE(NEO::CompressionSelector::preferRenderCompressedBuffer(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingUsmAllocationThenExpectCompressionEnabled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    GraphicsAllocation::AllocationType::BUFFER,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_TRUE(NEO::CompressionSelector::preferRenderCompressedBuffer(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingSvmGpuAllocationThenExpectCompressionEnabled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    GraphicsAllocation::AllocationType::SVM_GPU,
                                    deviceBitfield);

    EXPECT_TRUE(NEO::CompressionSelector::preferRenderCompressedBuffer(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingOtherAllocationThenExpectCompressionDisabled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
                                    deviceBitfield);

    EXPECT_FALSE(NEO::CompressionSelector::preferRenderCompressedBuffer(properties));
}

} // namespace ult
} // namespace L0
