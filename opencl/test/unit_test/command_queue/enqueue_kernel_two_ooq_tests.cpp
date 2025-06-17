/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"

using namespace NEO;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

struct TwoOOQsTwoDependentWalkers : public HelloWorldTest<OOQFixtureFactory>,
                                    public ClHardwareParse {
    typedef HelloWorldTest<OOQFixtureFactory> Parent;
    using Parent::createCommandQueue;
    using Parent::pCmdQ;
    using Parent::pDevice;
    using Parent::pKernel;

    TwoOOQsTwoDependentWalkers() {
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

        auto &commandStream = pCmdQ->getGpgpuCommandStreamReceiver().getCS(2048);
        auto pCommandStreamBuffer = reinterpret_cast<char *>(commandStream.getCpuBase());
        std::fill(pCommandStreamBuffer + commandStream.getUsed(), pCommandStreamBuffer + commandStream.getMaxAvailableSpace(), 0);

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

        // Create a second command queue (beyond the default one)
        pCmdQ2 = createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
        ASSERT_NE(nullptr, pCmdQ2);

        auto &commandStream2 = pCmdQ2->getGpgpuCommandStreamReceiver().getCS(2048);
        auto pCommandStreamBuffer2 = reinterpret_cast<char *>(commandStream2.getCpuBase());
        std::fill(pCommandStreamBuffer2 + commandStream2.getUsed(), pCommandStreamBuffer2 + commandStream2.getMaxAvailableSpace(), 0);

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
        pCmdQ->flush();
        pCmdQ2->flush();

        ClHardwareParse::parseCommands<FamilyType>(*pCmdQ);
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

    void hexDump(void *ptr, size_t size) {
        uint8_t *byte = reinterpret_cast<uint8_t *>(ptr);
        uint8_t bytesNum = 0;
        while (bytesNum < size) {
            std::cout << std::hex << "0x" << static_cast<uint32_t>(byte[bytesNum++]) << " ";
        }
        std::cout << std::endl;
    }

    GenCmdList::iterator itorWalker1;
    GenCmdList::iterator itorWalker2;
    CommandQueue *pCmdQ2 = nullptr;
};

HWTEST_F(TwoOOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenTwoDifferentWalkersAreCreated) {
    parseWalkers<FamilyType>();
    EXPECT_NE(itorWalker1, itorWalker2);
}

HWTEST2_F(TwoOOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipelineSelectExists, IsAtMostXeCore) {
    parseWalkers<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, TwoOOQsTwoDependentWalkers, GivenTwoCommandQueuesWhenEnqueuingKernelThenThereIsOneVfeState) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    parseWalkers<FamilyType>();

    auto commandsList = getCommandsList<MEDIA_VFE_STATE>();
    auto numCommands = commandsList.size();
    EXPECT_EQ(1u, numCommands);

    auto expectedCmd = FamilyType::cmdInitMediaVfeState;

    if (numCommands > 1) {
        uint32_t commandIndex = 0;
        for (auto &cmd : commandsList) {
            auto offset = reinterpret_cast<uint8_t *>(cmd) - reinterpret_cast<uint8_t *>(*cmdList.begin());
            std::cout << "MEDIA_VFE_STATE [" << commandIndex << "] : 0x" << std::hex << cmd << ". Byte offset in command buffer: 0x" << offset << std::endl;
            commandIndex++;
            if (memcmp(&expectedCmd, cmd, sizeof(MEDIA_VFE_STATE)) == 0) {
                std::cout << "matches expected MEDIA_VFE_STATE command" << std::endl;
            } else {
                std::cout << "doesn't match expected MEDIA_VFE_STATE command." << std::endl;
            }
            std::cout << "Expected:" << std::endl;
            hexDump(&expectedCmd, sizeof(MEDIA_VFE_STATE));
            std::cout << "Actual:" << std::endl;
            hexDump(cmd, sizeof(MEDIA_VFE_STATE));
        }
        std::cout << std::endl
                  << "Command buffer content:" << std::endl;
        auto it = cmdList.begin();
        uint32_t cmdNum = 0;
        std::string cmdBuffStr;
        while (it != cmdList.end()) {
            cmdBuffStr += std::to_string(cmdNum) + ":" + HardwareParse::getCommandName<FamilyType>(*it) + " ";
            ++cmdNum;
            ++it;
        }
        std::cout << cmdBuffStr << std::endl;
    }
}

HWTEST_F(TwoOOQsTwoDependentWalkers, DISABLED_GivenTwoCommandQueuesWhenEnqueuingKernelThenOnePipeControlIsInsertedBetweenWalkers) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;

    parseWalkers<FamilyType>();
    auto itorCmd = find<PIPE_CONTROL *>(itorWalker1, itorWalker2);

    // Should find a PC.
    EXPECT_NE(itorWalker2, itorCmd);
}
