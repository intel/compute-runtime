/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"

using namespace NEO;

using BlitterDispatcheTest = Test<DispatcherFixture>;

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForPreemptionCmdSizeThenReturnZero) {
    EXPECT_EQ(0u, BlitterDispatcher<FamilyType>::getSizePreemption());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingPreemptionCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType>::dispatchPreemption(cmdBuffer);

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForMonitorFenceCmdSizeThenReturnExpectedNumberOfMiFlush) {
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    EXPECT_EQ(expectedSize, BlitterDispatcher<FamilyType>::getSizeMonitorFence(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingMonitorFenceCmdThenDispatchMiFlushWithPostSync) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();

    uint64_t expectedGpuAddress = 0x5100ull;
    uint64_t expectedValue = 0x1234ull;
    BlitterDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, expectedGpuAddress, expectedValue, pDevice->getHardwareInfo(), false, false);

    EXPECT_EQ(expectedSize, cmdBuffer.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    auto commandsList = hwParse.getCommandsList<MI_FLUSH_DW>();

    bool foundPostSync = false;
    for (auto &cmd : commandsList) {
        MI_FLUSH_DW *miFlush = static_cast<MI_FLUSH_DW *>(cmd);
        if (miFlush->getDestinationAddress() == expectedGpuAddress &&
            miFlush->getImmediateData() == expectedValue) {
            foundPostSync = true;
        }
    }
    EXPECT_TRUE(foundPostSync);
}
HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForCacheFlushCmdSizeThenReturnExpetedSize) {
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    EXPECT_EQ(expectedSize, BlitterDispatcher<FamilyType>::getSizeCacheFlush(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingCacheFlushCmdThenDispatchMiFlushCommand) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();

    BlitterDispatcher<FamilyType>::dispatchCacheFlush(cmdBuffer, pDevice->getHardwareInfo(), 0ull);

    EXPECT_EQ(expectedSize, cmdBuffer.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    auto commandsList = hwParse.getCommandsList<MI_FLUSH_DW>();
    EXPECT_LE(1u, commandsList.size());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingTlbFlushThenDispatchMiFlushCommandWithproperBits) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();

    BlitterDispatcher<FamilyType>::dispatchTlbFlush(cmdBuffer, 0ull, *defaultHwInfo);

    EXPECT_EQ(expectedSize, cmdBuffer.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer, EncodeMiFlushDW<FamilyType>::getMiFlushDwWaSize());
    auto miFlushDw = hwParse.getCommand<MI_FLUSH_DW>();

    EXPECT_TRUE(miFlushDw->getTlbInvalidate());
    EXPECT_EQ(miFlushDw->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);

    EXPECT_EQ(BlitterDispatcher<FamilyType>::getSizeTlbFlush(), EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenCheckingForMultiTileSynchronizationSupportThenExpectFalse) {
    EXPECT_FALSE(BlitterDispatcher<FamilyType>::isMultiTileSynchronizationSupported());
}
