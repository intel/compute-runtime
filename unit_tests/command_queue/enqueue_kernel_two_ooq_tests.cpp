/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "test.h"

using namespace OCLRT;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

struct TwoOOQsTwoDependentWalkers : public HelloWorldTest<OOQFixtureFactory>,
                                    public HardwareParse {
    typedef HelloWorldTest<OOQFixtureFactory> Parent;
    using Parent::createCommandQueue;
    using Parent::pCmdQ;
    using Parent::pDevice;
    using Parent::pKernel;

    TwoOOQsTwoDependentWalkers() {
    }

    void SetUp() override {
        Parent::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        delete pCmdQ2;
        HardwareParse::TearDown();
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
        HardwareParse::parseCommands<FamilyType>(*pCmdQ);

        // Create a second command queue (beyond the default one)
        pCmdQ2 = createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
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
        HardwareParse::parseCommands<FamilyType>(*pCmdQ2);

        pCmdQ->flush();
        pCmdQ2->flush();

        Event *E1 = castToObject<Event>(event1);
        ASSERT_NE(nullptr, E1);
        Event *E2 = castToObject<Event>(event2);
        ASSERT_NE(nullptr, E2);
        delete E1;
        delete E2;

        typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
        itorWalker1 = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker1);

        itorWalker2 = itorWalker1;
        ++itorWalker2;
        itorWalker2 = find<GPGPU_WALKER *>(itorWalker2, cmdList.end());
        ASSERT_NE(cmdList.end(), itorWalker2);
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;
    CommandQueue *pCmdQ2 = nullptr;
};

HWTEST_F(TwoOOQsTwoDependentWalkers, shouldHaveTwoWalkers) {
    parseWalkers<FamilyType>();
    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST_F(TwoOOQsTwoDependentWalkers, shouldHaveOnePS) {
    parseWalkers<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, TwoOOQsTwoDependentWalkers, shouldHaveOneVFEState) {
    parseWalkers<FamilyType>();

    auto numCommands = getCommandsList<typename FamilyType::MEDIA_VFE_STATE>().size();
    EXPECT_EQ(1u, numCommands);
}

HWTEST_F(TwoOOQsTwoDependentWalkers, shouldHaveAPipecontrolBetweenWalkers) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    parseWalkers<FamilyType>();
    auto itorCmd = find<PIPE_CONTROL *>(itorWalker1, itorWalker2);

    // Should find a PC.
    EXPECT_NE(itorWalker2, itorCmd);
}
