/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/tbx/tbx_proto.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST(AubHelper, WhenGetPtEntryBitsIsCalledThenEntryBitsAreNotMasked) {
    uint64_t entryBits = makeBitMask<PageTableEntry::presentBit,
                                     PageTableEntry::writableBit,
                                     PageTableEntry::userSupervisorBit>();
    uint64_t maskedEntryBits = AubHelper::getPTEntryBits(entryBits);
    EXPECT_EQ(entryBits, maskedEntryBits);
}

TEST(AubHelper, GivenMultipleSubDevicesWhenGettingDeviceCountThenCorrectValueIsReturned) {
    DebugManagerStateRestore stateRestore;
    FeatureTable featureTable = {};
    WorkaroundTable workaroundTable = {};
    RuntimeCapabilityTable capTable = {};
    GT_SYSTEM_INFO sysInfo = {};
    PLATFORM platform = {};
    HardwareInfo hwInfo{&platform, &featureTable, &workaroundTable, &sysInfo, capTable};
    debugManager.flags.CreateMultipleSubDevices.set(2);

    uint32_t devicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 2u);

    debugManager.flags.CreateMultipleSubDevices.set(0);
    devicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 1u);
}

TEST(AubHelper, WhenMaskPTEntryBitsIsCalledThenLocalMemoryBitIsMasked) {
    uint64_t entryBits = makeBitMask<PageTableEntry::presentBit,
                                     PageTableEntry::writableBit,
                                     PageTableEntry::userSupervisorBit,
                                     PageTableEntry::localMemoryBit>();
    uint64_t maskedEntryBits = AubHelper::getPTEntryBits(entryBits);
    EXPECT_EQ(entryBits & ~makeBitMask<PageTableEntry::localMemoryBit>(), maskedEntryBits);
}

TEST(AubHelper, WhenHBMSizePerTileInGigabytesIsSetThenGetMemBankSizeReturnsCorrectValue) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.HBMSizePerTileInGigabytes.set(8);

    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &sysInfo = hwInfo.gtSystemInfo;
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);

    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 1;
    EXPECT_EQ(8 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));

    sysInfo.MultiTileArchInfo.TileCount = 2;
    EXPECT_EQ(8 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));

    sysInfo.MultiTileArchInfo.TileCount = 4;
    EXPECT_EQ(8 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));
}

TEST(AubHelper, givenAllocationTypeWhenAskingIfOneTimeWritableThenReturnCorrectResult) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocType = static_cast<AllocationType>(i);

        bool isOneTimeWritable = AubHelper::isOneTimeAubWritableAllocationType(allocType);

        switch (allocType) {
        case AllocationType::constantSurface:
        case AllocationType::globalSurface:
        case AllocationType::kernelIsa:
        case AllocationType::kernelIsaInternal:
        case AllocationType::privateSurface:
        case AllocationType::scratchSurface:
        case AllocationType::workPartitionSurface:
        case AllocationType::buffer:
        case AllocationType::bufferHostMemory:
        case AllocationType::image:
        case AllocationType::timestampPacketTagBuffer:
        case AllocationType::externalHostPtr:
        case AllocationType::mapAllocation:
        case AllocationType::svmGpu:
        case AllocationType::gpuTimestampDeviceBuffer:
        case AllocationType::assertBuffer:
        case AllocationType::tagBuffer:
        case AllocationType::syncDispatchToken:
        case AllocationType::hostFunction:
            EXPECT_TRUE(isOneTimeWritable);
            break;
        default:
            EXPECT_FALSE(isOneTimeWritable);
            break;
        }
    }
}

TEST(AubHelper, givenAlwaysAubWritableAndEnableTbxFaultManagerSetExternallyThenAllocationIsOneTimeAubWritableShouldReturnCorrectResult) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.EnableTbxPageFaultManager.set(0);
    NEO::debugManager.flags.SetCommandStreamReceiver.set(2);

    for (auto isAlwaysAubWritable : {false, true}) {
        for (auto isTbxFaultManagerEnabled : {false, true}) {
            NEO::debugManager.flags.SetBufferHostMemoryAlwaysAubWritable.set(isAlwaysAubWritable);
            NEO::debugManager.flags.EnableTbxPageFaultManager.set(isTbxFaultManagerEnabled);

            bool isOneTimeAubWritable = AubHelper::isOneTimeAubWritableAllocationType(AllocationType::bufferHostMemory);
            EXPECT_EQ(!isAlwaysAubWritable || isTbxFaultManagerEnabled, isOneTimeAubWritable);
        }
    }
}

using AubHelperTest = ::testing::Test;

HWTEST_F(AubHelperTest, WhenHBMSizePerTileInGigabytesIsNotSetThenGetMemBankSizeReturnsCorrectValue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &sysInfo = hwInfo.gtSystemInfo;
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);

    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 1;
    EXPECT_EQ(32 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));

    sysInfo.MultiTileArchInfo.TileCount = 2;
    EXPECT_EQ(16 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));

    sysInfo.MultiTileArchInfo.TileCount = 4;
    EXPECT_EQ(8 * MemoryConstants::gigaByte, AubHelper::getPerTileLocalMemorySize(&hwInfo, releaseHelper.get()));
}
