/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

#include "gtest/gtest.h"

using CommandStreamReceiverXe2AndLater = UltCommandStreamReceiverTest;

HWTEST2_F(CommandStreamReceiverXe2AndLater, GivenPreambleNotSentAndDebugFlagEnabledWhenFlushingTaskThenPipelineSelectIsSent, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.isPreambleSent = false;

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST2_F(CommandStreamReceiverXe2AndLater, GivenPreambleNotSentAndDebugFlagDisabledWhenFlushingTaskThenPipelineSelectIsNotSent, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(false);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_EQ(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}
