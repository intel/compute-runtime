/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

namespace L0 {
namespace ult {

TEST(CompressionSelectorL0Tests, GivenDefaultDebugFlagWhenProvidingUsmAllocationThenExpectCompressionDisabled) {
    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::buffer,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_FALSE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(CompressionSelectorL0Tests, GivenDisabledDebugFlagWhenProvidingUsmAllocationThenExpectCompressionDisabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableUsmCompression.set(0);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::buffer,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_FALSE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingUsmAllocationThenExpectCompressionEnabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::buffer,
                                    deviceBitfield);
    properties.flags.isUSMDeviceAllocation = 1u;

    EXPECT_TRUE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingSvmGpuAllocationThenExpectCompressionEnabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::svmGpu,
                                    deviceBitfield);

    EXPECT_TRUE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

TEST(CompressionSelectorL0Tests, GivenEnabledDebugFlagWhenProvidingOtherAllocationThenExpectCompressionDisabled) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableUsmCompression.set(1);

    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::bufferHostMemory,
                                    deviceBitfield);

    EXPECT_FALSE(NEO::CompressionSelector::preferCompressedAllocation(properties));
}

} // namespace ult
} // namespace L0
