/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/two_walker_fixture.h"
#include "runtime/helpers/kernel_commands.h"
#include "test.h"

using namespace OCLRT;

typedef TwoWalkerTest<HelloWorldFixtureFactory> IOQWithTwoWalkers;

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, shouldHaveTwoWalkers) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, shouldHaveOnePS) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, shouldHaveOneVFEState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, shouldHaveAPipecontrolBetweenWalkers2) {
    enqueueTwoKernels<FamilyType>();
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto WaNeeded = KernelCommandsHelper<FamilyType>::isPipeControlWArequired();

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
    EXPECT_EQ((uint64_t)commandStreamReceiver.getTagAddress(), addressPC);
    EXPECT_EQ(1u, pipeControl->getImmediateData());
}
