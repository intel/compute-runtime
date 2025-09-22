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
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/tbx/tbx_proto.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_lrca_helper.h"

using namespace NEO;

TEST(AubHelper, GivenZeroPdEntryBitsWhenGetMemTraceIsCalledThenTraceNonLocalIsReturned) {
    int hint = AubHelper::getMemTrace(0u);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, hint);
}

TEST(AubHelper, WhenGetPtEntryBitsIsCalledThenEntryBitsAreNotMasked) {
    uint64_t entryBits = BIT(PageTableEntry::presentBit) |
                         BIT(PageTableEntry::writableBit) |
                         BIT(PageTableEntry::userSupervisorBit);
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

TEST(AubHelper, WhenGetMemTraceIsCalledWithLocalMemoryPDEntryBitsThenTraceLocalIsReturned) {
    int hint = AubHelper::getMemTrace(BIT(PageTableEntry::localMemoryBit));
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, hint);
}

TEST(AubHelper, WhenMaskPTEntryBitsIsCalledThenLocalMemoryBitIsMasked) {
    uint64_t entryBits = BIT(PageTableEntry::presentBit) |
                         BIT(PageTableEntry::writableBit) |
                         BIT(PageTableEntry::userSupervisorBit) |
                         BIT(PageTableEntry::localMemoryBit);
    uint64_t maskedEntryBits = AubHelper::getPTEntryBits(entryBits);
    EXPECT_EQ(entryBits & ~BIT(PageTableEntry::localMemoryBit), maskedEntryBits);
}

TEST(AubHelper, WhenGetMemTypeIsCalledWithAGivenAddressSpaceThenCorrectMemTypeIsReturned) {
    uint32_t addressSpace = AubHelper::getMemType(AubMemDump::AddressSpaceValues::TraceLocal);
    EXPECT_EQ(MemType::local, addressSpace);

    addressSpace = AubHelper::getMemType(AubMemDump::AddressSpaceValues::TraceNonlocal);
    EXPECT_EQ(MemType::system, addressSpace);
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

using AubHelperHwTest = Test<DeviceFixture>;

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetDataHintForPml4EntryIsCalledThenTraceNotypeIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int dataHint = aubHelper.getDataHintForPml4Entry();
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceNotype, dataHint);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetDataHintForPdpEntryIsCalledThenTraceNotypeIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int dataHint = aubHelper.getDataHintForPdpEntry();
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceNotype, dataHint);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetDataHintForPdEntryIsCalledThenTraceNotypeIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int dataHint = aubHelper.getDataHintForPdEntry();
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceNotype, dataHint);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetDataHintForPtEntryIsCalledThenTraceNotypeIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int dataHint = aubHelper.getDataHintForPtEntry();
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceNotype, dataHint);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetMemTraceForPml4EntryIsCalledThenTracePml4EntryIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int addressSpace = aubHelper.getMemTraceForPml4Entry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TracePml4Entry, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetMemTraceForPdpEntryIsCalledThenTracePhysicalPdpEntryIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int addressSpace = aubHelper.getMemTraceForPdpEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TracePhysicalPdpEntry, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetMemTraceForPd4EntryIsCalledThenTracePpgttPdEntryIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int addressSpace = aubHelper.getMemTraceForPdEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TracePpgttPdEntry, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenDisabledLocalMemoryWhenGetMemTraceForPtEntryIsCalledThenTracePpgttEntryIsReturned) {
    AubHelperHw<FamilyType> aubHelper(false);
    int addressSpace = aubHelper.getMemTraceForPtEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TracePpgttEntry, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenEnabledLocalMemoryWhenGetMemTraceForPml4EntryIsCalledThenTraceLocalIsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    int addressSpace = aubHelper.getMemTraceForPml4Entry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenEnabledLocalMemoryWhenGetMemTraceForPdpEntryIsCalledThenTraceLocalIsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    int addressSpace = aubHelper.getMemTraceForPdpEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenEnabledLocalMemoryWhenGetMemTraceForPd4EntryIsCalledThenTraceLocalIsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    int addressSpace = aubHelper.getMemTraceForPdEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWTEST_F(AubHelperHwTest, GivenEnabledLocalMemoryWhenGetMemTraceForPtEntryIsCalledThenTraceLocalIsReturned) {
    AubHelperHw<FamilyType> aubHelper(true);
    int addressSpace = aubHelper.getMemTraceForPtEntry();
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWTEST_F(AubHelperHwTest, givenLrcaHelperWhenContextIsInitializedThenContextFlagsAreSet) {
    const auto &csTraits = CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(aub_stream::ENGINE_RCS);
    MockLrcaHelper lrcaHelper(csTraits.mmioBase);

    std::unique_ptr<void, std::function<void(void *)>> lrcaBase(alignedMalloc(csTraits.sizeLRCA, csTraits.alignLRCA), alignedFree);

    lrcaHelper.initialize(lrcaBase.get());
    ASSERT_NE(0u, lrcaHelper.setContextSaveRestoreFlagsCalled);
}
