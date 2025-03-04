/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
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
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    EXPECT_EQ(expectedSize, BlitterDispatcher<FamilyType>::getSizeMonitorFence(pDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingMonitorFenceCmdThenDispatchMiFlushWithPostSync) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};

    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);

    uint64_t expectedGpuAddress = 0x5100ull;
    uint64_t expectedValue = 0x1234ull;
    BlitterDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, expectedGpuAddress, expectedValue, pDevice->getRootDeviceEnvironment(), false, false, false);

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
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    EXPECT_EQ(expectedSize, BlitterDispatcher<FamilyType>::getSizeCacheFlush(pDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingCacheFlushCmdThenDispatchMiFlushCommand) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);

    BlitterDispatcher<FamilyType>::dispatchCacheFlush(cmdBuffer, pDevice->getRootDeviceEnvironment(), 0ull);

    EXPECT_EQ(expectedSize, cmdBuffer.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer);
    auto commandsList = hwParse.getCommandsList<MI_FLUSH_DW>();
    EXPECT_LE(1u, commandsList.size());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingTlbFlushThenDispatchMiFlushCommandWithproperBits) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    size_t expectedSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);

    BlitterDispatcher<FamilyType>::dispatchTlbFlush(cmdBuffer, 0ull, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedSize, cmdBuffer.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdBuffer, MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs));
    auto miFlushDw = hwParse.getCommand<MI_FLUSH_DW>();

    EXPECT_TRUE(miFlushDw->getTlbInvalidate());
    EXPECT_EQ(miFlushDw->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(BlitterDispatcher<FamilyType>::getSizeTlbFlush(pDevice->getRootDeviceEnvironment()), EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenCheckingForMultiTileSynchronizationSupportThenExpectFalse) {
    EXPECT_FALSE(BlitterDispatcher<FamilyType>::isMultiTileSynchronizationSupported());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingIsCopyThenReturnTrue) {
    EXPECT_TRUE(BlitterDispatcher<FamilyType>::isCopy());
}
