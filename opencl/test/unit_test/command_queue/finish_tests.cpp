/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

struct FinishFixture : public ClDeviceFixture,
                       public CommandQueueHwFixture,
                       public CommandStreamFixture,
                       public HardwareParse {

    void setUp() {
        ClDeviceFixture::setUp();
        CommandQueueHwFixture::setUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::setUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        HardwareParse::setUp();
    }

    void tearDown() {
        HardwareParse::tearDown();
        CommandStreamFixture::tearDown();
        CommandQueueHwFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

typedef Test<FinishFixture> FinishTest;

HWTEST_F(FinishTest, GivenCsGreaterThanCqWhenFinishIsCalledThenPipeControlIsNotAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // HW = 1, CQ = 1, CS = 2 (last PC was 1)
    // This means there is no work in CQ that needs a PC
    uint32_t originalHwTag = 1;
    uint32_t originalCSRLevel = 2;
    uint32_t originalCQLevel = 1;
    *commandStreamReceiver.getTagAddress() = originalHwTag;
    commandStreamReceiver.taskLevel = originalCSRLevel; // Must be greater than or equal to HW
    pCmdQ->taskLevel = originalCQLevel;

    auto retVal = pCmdQ->finish(false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Don't need to artificially execute PIPE_CONTROL.
    // Nothing should have been sent
    //*pCS->getTagAddress() = originalCSRLevel;

    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), originalCSRLevel);
    EXPECT_EQ(pCmdQ->taskLevel, originalCQLevel);
    EXPECT_GE(pCmdQ->getHwTag(), pCmdQ->taskLevel);

    auto sizeUsed = pCS->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, pCmdBuffer, sizeUsed));

    auto itorCmd = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(FinishTest, WhenFinishIsCalledThenPipeControlIsNotAddedToCqCommandStream) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto retVal = pCmdQ->finish(false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Check for PIPE_CONTROL
    parseCommands<FamilyType>(pCmdQ->getCS(1024));
    auto itorCmd = reverseFind<PIPE_CONTROL *>(cmdList.rbegin(), cmdList.rend());
    EXPECT_EQ(cmdList.rend(), itorCmd);
}

HWTEST_F(FinishTest, givenFreshQueueWhenFinishIsCalledThenCommandStreamIsNotAllocated) {
    MockContext contextWithMockCmdQ(pClDevice, true);
    MockCommandQueueHw<FamilyType> cmdQ(&contextWithMockCmdQ, pClDevice, 0);

    auto retVal = cmdQ.finish(false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, cmdQ.peekCommandStream());
}

HWTEST_F(FinishTest, givenL3FlushAfterPostSyncEnabledWhenFlushTagUpdateIsCalledThenPipeControlIsAddedWithDcFlushAndTaskCountIsUpdated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(true);

    auto &productHelper = pClDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    MockContext contextWithMockCmdQ(pClDevice, true);
    MockCommandQueueHw<FamilyType> cmdQ(&contextWithMockCmdQ, pClDevice, 0);

    cmdQ.setPendingL3FlushForHostVisibleResources(true);
    cmdQ.l3FlushAfterPostSyncEnabled = true;

    auto &csr = cmdQ.getUltCommandStreamReceiver();
    auto used = csr.commandStream.getUsed();

    auto taskCountBeforeFinish = csr.taskCount.load();
    auto beforeWaitForAllEnginesCalledCount = cmdQ.waitForAllEnginesCalledCount;
    auto retVal = cmdQ.finish(true);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(taskCountBeforeFinish + 1, cmdQ.latestTaskCountWaited);
    EXPECT_FALSE(cmdQ.recordedSkipWait);
    EXPECT_EQ(beforeWaitForAllEnginesCalledCount + 1, cmdQ.waitForAllEnginesCalledCount);
    EXPECT_EQ(taskCountBeforeFinish + 1, csr.taskCount.load());
    EXPECT_EQ(taskCountBeforeFinish + 1, cmdQ.taskCount);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.commandStream, used);
    auto itorCmd = find<PIPE_CONTROL *>(hwParse.cmdList.begin(), hwParse.cmdList.end());

    EXPECT_NE(hwParse.cmdList.end(), itorCmd);

    // Verify DC flush is enabled
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_EQ(taskCountBeforeFinish + 1, pipeControl->getImmediateData());
}

HWTEST_F(FinishTest, givenL3FlushDeferredIfNeededAndL3FlushAfterPostSyncEnabledWhenCpuDataTransferHandlerCalledThenPipeControlWithDcFlushIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(true);

    auto &productHelper = pClDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    MockContext contextWithMockCmdQ(pClDevice, true);
    MockCommandQueueHw<FamilyType> cmdQ(&contextWithMockCmdQ, pClDevice, 0);
    cmdQ.setPendingL3FlushForHostVisibleResources(true);
    cmdQ.l3FlushAfterPostSyncEnabled = true;

    size_t offset = 0;
    size_t size = 16;
    MockGraphicsAllocation alloc{};
    auto buffer = std::make_unique<MockBuffer>(&contextWithMockCmdQ, alloc);
    auto mem = std::make_unique<uint8_t[]>(size);
    buffer->hostPtr = mem.get();
    buffer->memoryStorage = mem.get();
    auto dstPtr = std::make_unique<uint8_t[]>(size);
    auto &csr = cmdQ.getUltCommandStreamReceiver();
    auto usedBefore = csr.commandStream.getUsed();

    TransferProperties transferProperties(buffer.get(), CL_COMMAND_READ_BUFFER, 0, true, &offset, &size, dstPtr.get(), true, pDevice->getRootDeviceIndex());
    cl_event returnEvent = nullptr;
    EventsRequest eventsRequest(0, nullptr, &returnEvent);
    cl_int retVal = CL_SUCCESS;

    auto taskCountBeforeFinish = csr.taskCount.load();
    auto beforeWaitForAllEnginesCalledCount = cmdQ.waitForAllEnginesCalledCount;

    cmdQ.cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(taskCountBeforeFinish + 1, cmdQ.latestTaskCountWaited);
    EXPECT_FALSE(cmdQ.recordedSkipWait);
    EXPECT_EQ(beforeWaitForAllEnginesCalledCount + 1, cmdQ.waitForAllEnginesCalledCount);
    EXPECT_EQ(taskCountBeforeFinish + 1, csr.taskCount.load());
    EXPECT_EQ(taskCountBeforeFinish + 1, cmdQ.taskCount);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.commandStream, usedBefore);
    auto itPc = find<PIPE_CONTROL *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    EXPECT_NE(hwParse.cmdList.end(), itPc);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itPc);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(true, pipeControl->getDcFlushEnable());
    EXPECT_EQ(taskCountBeforeFinish + 1, pipeControl->getImmediateData());

    clReleaseEvent(returnEvent);
}