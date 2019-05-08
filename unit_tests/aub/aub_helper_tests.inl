/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/device_helpers.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_lrca_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(AubHelper, WhenGetMemTraceIsCalledWithZeroPDEntryBitsThenTraceNonLocalIsReturned) {
    int hint = AubHelper::getMemTrace(0u);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, hint);
}

TEST(AubHelper, WhenGetPTEntryBitsIsCalledThenEntryBitsAreNotMasked) {
    uint64_t entryBits = BIT(PageTableEntry::presentBit) |
                         BIT(PageTableEntry::writableBit) |
                         BIT(PageTableEntry::userSupervisorBit);
    uint64_t maskedEntryBits = AubHelper::getPTEntryBits(entryBits);
    EXPECT_EQ(entryBits, maskedEntryBits);
}

TEST(AubHelper, WhenCreateMultipleDevicesIsSetThenGetDevicesCountReturnedCorrectValue) {
    DebugManagerStateRestore stateRestore;
    FeatureTable featureTable = {};
    WorkaroundTable workaroundTable = {};
    RuntimeCapabilityTable capTable = {};
    GT_SYSTEM_INFO sysInfo = {};
    PLATFORM platform = {};
    HardwareInfo hwInfo{&platform, &featureTable, &workaroundTable, &sysInfo, capTable};
    DebugManager.flags.CreateMultipleDevices.set(2);

    uint32_t devicesCount = DeviceHelper::getDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 2u);

    DebugManager.flags.CreateMultipleDevices.set(0);
    devicesCount = DeviceHelper::getDevicesCount(&hwInfo);
    EXPECT_EQ(devicesCount, 1u);
}

typedef Test<DeviceFixture> AubHelperHwTest;

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

HWTEST_F(AubHelperHwTest, giverLrcaHelperWhenContextIsInitializedThenContextFlagsAreSet) {
    const auto &csTraits = CommandStreamReceiverSimulatedCommonHw<FamilyType>::getCsTraits(aub_stream::ENGINE_RCS);
    MockLrcaHelper lrcaHelper(csTraits.mmioBase);

    std::unique_ptr<void, std::function<void(void *)>> lrcaBase(alignedMalloc(csTraits.sizeLRCA, csTraits.alignLRCA), alignedFree);

    lrcaHelper.initialize(lrcaBase.get());
    ASSERT_NE(0u, lrcaHelper.setContextSaveRestoreFlagsCalled);
}
