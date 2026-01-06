/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/two_walker_fixture.h"

using namespace NEO;

typedef TwoWalkerTest<HelloWorldFixtureFactory> IOQWithTwoWalkers;

HWTEST_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenTwoDifferentWalkersAreCreated) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST2_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipelineSelectExists, IsAtMostXeCore) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenThereIsOneVfeState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(IOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipeControlIsInsertedBetweenWalkers) {

    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;

    enqueueTwoKernels<FamilyType>();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto waNeeded = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker1, itorWalker2);
    ASSERT_NE(itorWalker2, itorCmd);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorCmd);

    if (waNeeded) {
        EXPECT_EQ(0u, pipeControl->getPostSyncOperation());
        itorCmd++;
        itorCmd = find<PIPE_CONTROL *>(itorCmd, itorWalker2);
    }

    pipeControl = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, pipeControl);

    // We should be writing a tag value to an address
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
    // The PC address should match the CS tag address
    EXPECT_EQ(commandStreamReceiver.getTagAllocation()->getGpuAddress(), NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(commandStreamReceiver.heaplessStateInitialized ? 2u : 1u, pipeControl->getImmediateData());
}

HWTEST_F(IOQWithTwoWalkers, GivenStateCacheInvalidationWaIsRequiredWhenTwoKernelsWithStatefulAccessAreEnqueuedThenPipeControlWithStateCacheInvalidationIsInsertedBetweenWalkers) {
    NEO::ArgDescriptor ptrArg(NEO::ArgDescriptor::argTPointer);
    auto &explicitArgs = const_cast<KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.explicitArgs;
    explicitArgs.clear();
    explicitArgs.push_back(ptrArg);
    enqueueTwoKernels<FamilyType>();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    using COMPUTE_WALKER = typename FamilyType::DefaultWalkerType;

    auto walkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, walkers.size());

    auto itorPC = findAll<PIPE_CONTROL *>(walkers[0], walkers[1]);

    bool foundStateCacheInvalidation = false;
    for (auto it : itorPC) {
        auto pcCmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (pcCmd->getStateCacheInvalidationEnable()) {
            foundStateCacheInvalidation = true;
            break;
        }
    }

    auto releaseHelper = pClDevice->getDevice().getReleaseHelper();
    if (releaseHelper && releaseHelper->isStateCacheInvalidationWaRequired()) {
        EXPECT_TRUE(foundStateCacheInvalidation);
    } else {
        EXPECT_FALSE(foundStateCacheInvalidation);
    }
}

HWTEST_F(IOQWithTwoWalkers, GivenStateCacheInvalidationWaIsRequiredWhenTwoKernelsWithoutStatefulAccessAreEnqueuedThenPipeControlWithStateCacheInvalidationIsNotInsertedBetweenWalkers) {
    enqueueTwoKernels<FamilyType>();

    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    using COMPUTE_WALKER = typename FamilyType::DefaultWalkerType;

    auto walkers = findAll<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, walkers.size());

    auto itorPC = findAll<PIPE_CONTROL *>(walkers[0], walkers[1]);

    bool foundStateCacheInvalidation = false;
    for (auto it : itorPC) {
        auto pcCmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (pcCmd->getStateCacheInvalidationEnable()) {
            foundStateCacheInvalidation = true;
            break;
        }
    }

    EXPECT_FALSE(foundStateCacheInvalidation);
}
