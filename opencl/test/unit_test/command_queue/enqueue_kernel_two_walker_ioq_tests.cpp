/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

#include "opencl/test/unit_test/fixtures/two_walker_fixture.h"
#include "test.h"

using namespace NEO;

typedef TwoWalkerTest<HelloWorldFixtureFactory> IOQWithTwoWalkers;

HWTEST_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenTwoDifferentWalkersAreCreated) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipelineSelectExists) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenThereIsOneVfeState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipeControlIsInsertedBetweenWalkers) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;

    enqueueTwoKernels<FamilyType>();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto WaNeeded = MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo());

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
