/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "gtest/gtest.h"

#include <vector>

using namespace OCLRT;

typedef HelloWorldTest<HelloWorldFixtureFactory> DrmRequirementsTests;

template <typename FamilyType>
class CommandStreamReceiverDrmMock : public UltCommandStreamReceiver<FamilyType> {
  private:
    typedef UltCommandStreamReceiver<FamilyType> BaseClass;

    std::vector<GraphicsAllocation *> toFree; // pointers to be freed on destruction
  public:
    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
        EXPECT_NE(nullptr, batchBuffer.commandBufferAllocation->getUnderlyingBuffer());

        toFree.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.stream->replaceBuffer(nullptr, 0);
        batchBuffer.stream->replaceGraphicsAllocation(nullptr);
        return 0;
    }

    CommandStreamReceiverDrmMock<FamilyType>(ExecutionEnvironment &executionEnvironment)
        : BaseClass(*platformDevices[0], executionEnvironment) {
    }

    ~CommandStreamReceiverDrmMock() override {
        EXPECT_GT(toFree.size(), 0u); //make sure flush was called
        auto memoryManager = this->getMemoryManager();
        //Now free memory. if CQ/CSR did the same, we will hit double-free
        for (auto p : toFree)
            memoryManager->freeGraphicsMemory(p);
    }
};

HWTEST_F(DrmRequirementsTests, enqueueKernel) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};

    auto pCommandStreamReceiver = new CommandStreamReceiverDrmMock<FamilyType>(*pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(DrmRequirementsTests, finishTwice) {
    CommandStreamReceiver *pCommandStreamReceiver = new CommandStreamReceiverDrmMock<FamilyType>(*pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCommandStreamReceiver);
    size_t GWS = 1;
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);

    // enqueue something that can be finished...
    auto retVal = pCmdQ->enqueueKernel(pKernel,
                                       1,
                                       0,
                                       &GWS,
                                       nullptr,
                                       0,
                                       nullptr,
                                       nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->finish(false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->finish(false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(DrmRequirementsTests, enqueueKernelFinish) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};

    CommandStreamReceiver *pCommandStreamReceiver = new CommandStreamReceiverDrmMock<FamilyType>(*pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    // Replace the legacy command streamer with the Drm file version
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);

    auto retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->enqueueKernel(
        pKernel,
        1,
        globalWorkOffset,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->finish(false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(DrmRequirementsTests, csrNewCS) {
    CommandStreamReceiver *pCSR = new UltCommandStreamReceiver<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCSR);
    pCSR->setMemoryManager(pDevice->getMemoryManager());
    auto memoryManager = pCSR->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);
    {
        auto &cs = pCSR->getCS();
        EXPECT_NE(nullptr, cs.getCpuBase());
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        memoryManager->freeGraphicsMemory(cs.getGraphicsAllocation());
        cs.replaceBuffer(nullptr, 0);
        cs.replaceGraphicsAllocation(nullptr);
    }
    {
        auto &cs = pCSR->getCS();
        EXPECT_NE(nullptr, cs.getCpuBase());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GE(cs.getGraphicsAllocation()->getUnderlyingBufferSize(), cs.getMaxAvailableSpace());
    }

    delete pCSR;
}

HWTEST_F(DrmRequirementsTests, csrNewCSSized) {
    CommandStreamReceiver *pCSR = new UltCommandStreamReceiver<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, pCSR);
    pCSR->setMemoryManager(pDevice->getMemoryManager());
    auto memoryManager = pCSR->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);
    {
        auto &cs = pCSR->getCS(128);
        EXPECT_NE(nullptr, cs.getCpuBase());
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        memoryManager->freeGraphicsMemory(cs.getGraphicsAllocation());
        cs.replaceBuffer(nullptr, 0);
        cs.replaceGraphicsAllocation(nullptr);
    }
    {
        auto &cs = pCSR->getCS(128);
        EXPECT_NE(nullptr, cs.getCpuBase());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GE(cs.getGraphicsAllocation()->getUnderlyingBufferSize(), cs.getMaxAvailableSpace());
    }

    delete pCSR;
}

TEST_F(DrmRequirementsTests, cqNewCS) {
    {
        auto &cs = pCmdQ->getCS(1024);
        auto memoryManager = pDevice->getMemoryManager();
        EXPECT_NE(nullptr, cs.getCpuBase());
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        memoryManager->freeGraphicsMemory(cs.getGraphicsAllocation());
        cs.replaceBuffer(nullptr, 0);
        cs.replaceGraphicsAllocation(nullptr);
    }
    {
        auto &cs = pCmdQ->getCS(1024);
        EXPECT_NE(nullptr, cs.getCpuBase());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GE(cs.getGraphicsAllocation()->getUnderlyingBufferSize(), cs.getMaxAvailableSpace());
    }
}

TEST_F(DrmRequirementsTests, cqNewCSSized) {
    {
        auto &cs = pCmdQ->getCS(128);
        auto memoryManager = pDevice->getMemoryManager();
        EXPECT_NE(nullptr, cs.getCpuBase());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        memoryManager->freeGraphicsMemory(cs.getGraphicsAllocation());
        cs.replaceBuffer(nullptr, 0);
        cs.replaceGraphicsAllocation(nullptr);
    }
    {
        auto &cs = pCmdQ->getCS(128);
        EXPECT_NE(nullptr, cs.getCpuBase());
        EXPECT_GT(cs.getMaxAvailableSpace(), 0u);
        ASSERT_NE(nullptr, cs.getGraphicsAllocation());
        EXPECT_GE(cs.getGraphicsAllocation()->getUnderlyingBufferSize(), cs.getMaxAvailableSpace());
    }
}
