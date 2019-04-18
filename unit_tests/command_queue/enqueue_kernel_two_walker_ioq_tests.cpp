/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/fixtures/two_walker_fixture.h"

using namespace NEO;

typedef TwoWalkerTest<HelloWorldFixtureFactory> IOQWithTwoWalkers;

HWTEST_F(IOQWithTwoWalkers, shouldHaveTwoWalkers) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST_F(IOQWithTwoWalkers, shouldHaveOnePS) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, shouldHaveOneVFEState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(IOQWithTwoWalkers, shouldHaveAPipecontrolBetweenWalkers2) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;

    enqueueTwoKernels<FamilyType>();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto WaNeeded = HardwareCommandsHelper<FamilyType>::isPipeControlWArequired();

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker1, itorWalker2);
    ASSERT_NE(itorWalker2, itorCmd);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorCmd);

    if (WaNeeded) {
        EXPECT_EQ(0u, pipeControl->getPostSyncOperation());
        itorCmd++;
        itorCmd = find<PIPE_CONTROL *>(itorCmd, itorWalker2);
    }

    pipeControl = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, pipeControl);

    // We should be writing a tag value to an address
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    uint64_t addressPC = ((uint64_t)pipeControl->getAddressHigh() << 32) | pipeControl->getAddress();

    // The PC address should match the CS tag address
    EXPECT_EQ(commandStreamReceiver.getTagAllocation()->getGpuAddress(), addressPC);
    EXPECT_EQ(1u, pipeControl->getImmediateData());
}
