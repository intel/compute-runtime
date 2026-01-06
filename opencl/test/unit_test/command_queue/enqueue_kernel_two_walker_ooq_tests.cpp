/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/two_walker_fixture.h"

using namespace NEO;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

typedef TwoWalkerTest<OOQFixtureFactory> OOQWithTwoWalkers;

HWTEST_F(OOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenTwoDifferentWalkersAreCreated) {
    enqueueTwoKernels<FamilyType>();

    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST2_F(OOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipelineSelectExists, IsAtMostXeCore) {
    enqueueTwoKernels<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, OOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenThereIsOneVfeState) {
    enqueueTwoKernels<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(OOQWithTwoWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipeControlIsInsertedBetweenWalkers) {
    enqueueTwoKernels<FamilyType>();

    auto itorCmd = find<typename FamilyType::PIPE_CONTROL *>(itorWalker1, itorWalker2);
    // Workaround for DRM i915 coherency patch
    // EXPECT_EQ(itorWalker2, itorCmd);

    if (pCmdQ->getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled()) {
        EXPECT_EQ(itorWalker2, itorCmd);
    } else {
        EXPECT_NE(itorWalker2, itorCmd);
    }
}
