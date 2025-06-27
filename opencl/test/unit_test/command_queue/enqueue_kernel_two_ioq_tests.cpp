/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"

using namespace NEO;

struct TwoIOQsTwoDependentWalkers : public HelloWorldTest<HelloWorldFixtureFactory>,
                                    public ClHardwareParse {
    typedef HelloWorldTest<HelloWorldFixtureFactory> Parent;
    using Parent::createCommandQueue;
    using Parent::pCmdQ;
    using Parent::pDevice;
    using Parent::pKernel;

    TwoIOQsTwoDependentWalkers() {
    }

    void SetUp() override {
        Parent::SetUp();
        ClHardwareParse::setUp();
    }

    void TearDown() override {
        delete pCmdQ2;
        ClHardwareParse::tearDown();
        Parent::TearDown();
    }

    template <typename FamilyType>
    void parseWalkers() {
        cl_uint workDim = 1;
        size_t globalWorkOffset[3] = {0, 0, 0};
        size_t globalWorkSize[3] = {1, 1, 1};
        size_t localWorkSize[3] = {1, 1, 1};
        cl_event event1 = nullptr;
        cl_event event2 = nullptr;

        auto retVal = pCmdQ->enqueueKernel(
            pKernel,
            workDim,
            globalWorkOffset,
            globalWorkSize,
            localWorkSize,
            0,
            nullptr,
            &event1);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ClHardwareParse::parseCommands<FamilyType>(*pCmdQ);

        // Create a second command queue (beyond the default one)
        pCmdQ2 = createCommandQueue(pClDevice);
        ASSERT_NE(nullptr, pCmdQ2);

        retVal = pCmdQ2->enqueueKernel(
            pKernel,
            workDim,
            globalWorkOffset,
            globalWorkSize,
            localWorkSize,
            1,
            &event1,
            &event2);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ClHardwareParse::parseCommands<FamilyType>(*pCmdQ2);

        Event *e1 = castToObject<Event>(event1);
        ASSERT_NE(nullptr, e1);
        Event *e2 = castToObject<Event>(event2);
        ASSERT_NE(nullptr, e2);
        delete e1;
        delete e2;

        itorWalker1 = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);

        itorWalker2 = itorWalker1;
        ++itorWalker2;
        itorWalker2 = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(itorWalker2, cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker2);
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;
    CommandQueue *pCmdQ2 = nullptr;
};

HWTEST_F(TwoIOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenTwoDifferentWalkersAreCreated) {
    parseWalkers<FamilyType>();
    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST2_F(TwoIOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipelineSelectExists, IsAtMostXeCore) {
    parseWalkers<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, TwoIOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenThereIsOneVfeState) {
    parseWalkers<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(TwoIOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipeControlIsInsertedBetweenWalkers) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    parseWalkers<FamilyType>();
    auto itorCmd = find<PIPE_CONTROL *>(itorWalker1, itorWalker2);

    // Should find a PC.

    if (pCmdQ2->getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled()) {
        EXPECT_EQ(itorWalker2, itorCmd);
    } else {
        EXPECT_NE(itorWalker2, itorCmd);
    }
}
