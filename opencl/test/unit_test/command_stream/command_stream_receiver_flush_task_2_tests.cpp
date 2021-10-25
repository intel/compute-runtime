/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "test.h"

#include "reg_configs_common.h"

using namespace NEO;

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockedKernelNotRequiringDCFlushWhenUnblockedThenDCFlushIsNotAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    MockContext ctx(pClDevice);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 1, 2};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    auto &commandStreamCSR = commandStreamReceiver.getCS();

    commandQueue.enqueueWriteBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 1, &blockingEvent, 0);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);
    auto &commandStreamTask = *commandStreamReceiver.lastFlushedCommandStream;

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
    }

    // Verify that the dcFlushEnabled bit is set in PC
    auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pCmdWA->getDcFlushEnable());

    buffer->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenTaskCsPassedAsCommandStreamParamWhenFlushingTaskThenCompletionStampIsCorrect) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &commandStreamTask = commandQueue.getCS(1024);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    // Pass taskCS as command stream parameter
    auto cs = commandStreamReceiver.flushTask(
        commandStreamTask,
        0,
        dsh,
        ioh,
        ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    // Verify that flushTask returned a valid completion stamp
    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), cs.taskCount);
    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), cs.taskLevel);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEmptyQueueWhenFinishingThenTaskCountIsNotIncremented) {
    MockContext ctx(pClDevice);
    MockCommandQueueHw<FamilyType> mockCmdQueue(&ctx, pClDevice, nullptr);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint32_t taskCount = 0;
    taskLevel = taskCount;
    mockCmdQueue.taskCount = taskCount;
    mockCmdQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    mockCmdQueue.finish();
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    mockCmdQueue.finish();
    //nothings sent to the HW, no need to bump tags
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCmdQueue.latestTaskCountWaited);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenTaskCountToWaitBiggerThanLatestSentTaskCountWhenWaitForCompletionThenFlushPipeControl) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    csr.waitForCompletionWithTimeout(false, 0, 1u);

    auto &commandStreamTask = csr.getCS();
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenNonDcFlushWithInitialTaskCountZeroWhenFinishingThenTaskCountIncremented) {
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    MockCommandQueueHw<FamilyType> mockCmdQueue(&ctx, pClDevice, nullptr);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    mockCmdQueue.taskCount = taskCount;
    mockCmdQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish after enqueued kernel(cmdq task count = 1)
    mockCmdQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    mockCmdQueue.finish();
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, mockCmdQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    // finish again - dont call flush task
    mockCmdQueue.finish();
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, mockCmdQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenDcFlushWhenFinishingThenTaskCountIncremented) {
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    MockCommandQueueHw<FamilyType> mockCmdQueue(&ctx, pClDevice, nullptr);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;
    size_t tempBuffer[] = {0, 1, 2};
    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    mockCmdQueue.taskCount = taskCount;
    mockCmdQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish(dcFlush=true) from blocking MapBuffer after enqueued kernel
    mockCmdQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    auto ptr = mockCmdQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    // cmdQ task count = 2, finish again
    mockCmdQueue.finish();

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish again - dont flush task again
    mockCmdQueue.finish();

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish(dcFlush=true) from MapBuffer again - dont call FinishTask n finished queue
    retVal = mockCmdQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ptr = mockCmdQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    //cleanup
    retVal = mockCmdQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenPowerOfTwoGlobalWorkSizeAndNullLocalWorkgroupSizeWhenEnqueueKernelIsCalledThenGpGpuWalkerHasOptimalSIMDmask) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    size_t GWS = 1024;
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    auto &commandStreamTask = commandQueue.getCS(1024);
    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorCmd, cmdList.end());
    auto cmdGpGpuWalker = genCmdCast<GPGPU_WALKER *>(*itorCmd);

    //execution masks should be all active
    EXPECT_EQ(0xffffffffu, cmdGpGpuWalker->getBottomExecutionMask());
    EXPECT_EQ(0xffffffffu, cmdGpGpuWalker->getRightExecutionMask());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEventIsQueriedWhenEnqueuingThenTaskCountIncremented) {
    MockContext ctx(pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    cl_event event = nullptr;
    Event *pEvent;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {5, 5, 5};

    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 0, 0, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    pEvent = (Event *)event;

    retVal = Event::waitForEvents(1, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    retVal = clReleaseEvent(pEvent);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenNonBlockingMapEnqueueWhenFinishingThenNothingIsSubmittedToTheHardware) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t tempBuffer[] = {0, 1, 2};
    cl_int retVal;

    AlignedBuffer mockBuffer;

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    auto ptr = commandQueue.enqueueMapBuffer(&mockBuffer, CL_FALSE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, ptr);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.finish();

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    auto &commandStreamTask = commandQueue.getCS(1024);
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests,
            GivenFlushedCallRequiringDCFlushWhenBlockingEnqueueIsCalledThenPipeControlWithDCFlushIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pClDevice);
    MockCommandQueueHw<FamilyType> mockCmdQueue(&ctx, pClDevice, nullptr);
    cl_event event = nullptr;

    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
    auto &commandStreamTask = mockCmdQueue.getCS(1024);

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {5, 5, 5};

    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // Call requiring DCFlush, nonblocking
    buffer->forceDisallowCPUCopy = true;
    mockCmdQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 0, 0, 0);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    mockCmdQueue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 0, 0, &event);

    EXPECT_EQ(2u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(2u, mockCmdQueue.latestTaskCountWaited);

    // Parse command list to verify that PC was added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorWalker = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorCmd);

    auto cmdPC = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, cmdPC);

    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        // SKL: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_FALSE(cmdPC->getDcFlushEnable());
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmdPC = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), cmdPC->getDcFlushEnable());
    } else {
        // single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), cmdPC->getDcFlushEnable());
    }

    retVal = clReleaseEvent(event);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDefaultCommandStreamReceiverThenRoundRobinPolicyIsSelected) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(HwHelperHw<FamilyType>::get().getDefaultThreadArbitrationPolicy(), commandStreamReceiver.peekThreadArbitrationPolicy());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenKernelWithSlmWhenPreviousSLML3WasSentThenDontProgramL3) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t GWS = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    uint32_t L3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);

    // Mark Pramble as sent, override L3Config to SLM config
    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = L3Config;
    commandStreamReceiver->lastSentThreadArbitrationPolicy = kernel.mockKernel->getThreadArbitrationPolicy();

    ((MockKernel *)kernel)->setTotalSLMSize(1024);

    cmdList.clear();
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list to verify that PC was added to taskCS
    parseCommands<FamilyType>(commandStreamCSR, 0);

    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<FamilyType>::registerOffset);
    EXPECT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenCreatingCommandStreamReceiverHwThenValidPointerIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    auto csrHw = CommandStreamReceiverHw<FamilyType>::create(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_NE(nullptr, csrHw);

    GmmPageTableMngr *ptm = csrHw->createPageTableManager();
    EXPECT_EQ(nullptr, ptm);
    delete csrHw;

    DebugManager.flags.SetCommandStreamReceiver.set(0);
    int32_t GetCsr = DebugManager.flags.SetCommandStreamReceiver.get();
    EXPECT_EQ(0, GetCsr);

    auto csr = NEO::createCommandStream(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_NE(nullptr, csr);
    delete csr;
    DebugManager.flags.SetCommandStreamReceiver.set(0);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenFlushingThenScratchAllocationIsReused) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSizes(1024, 0); // whatever > 0

    flushTask(*commandStreamReceiver);

    auto tagAllocation = commandStreamReceiver->getTagAllocation();
    auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
    ASSERT_NE(tagAllocation, nullptr);
    ASSERT_NE(scratchAllocation, nullptr);

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));

    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));

    // call makeResident on tag and scratch allocations per each flush
    // DONT skip residency calls when scratch allocation is the same(new required size <= previous size)
    commandStreamReceiver->madeResidentGfxAllocations.clear(); // this is only history - we can clean this
    commandStreamReceiver->madeNonResidentGfxAllocations.clear();

    flushTask(*commandStreamReceiver); // 2nd flush

    auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_EQ(scratchAllocation, newScratchAllocation); // Allocation unchanged. Dont skip residency handling

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));

    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCommandStreamReceiverWhenFenceAllocationIsRequiredAndFlushTaskIsCalledThenFenceAllocationIsMadeResident) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    auto globalFenceAllocation = commandStreamReceiver->globalFenceAllocation;
    ASSERT_NE(globalFenceAllocation, nullptr);

    EXPECT_FALSE(commandStreamReceiver->isMadeResident(globalFenceAllocation));
    EXPECT_FALSE(commandStreamReceiver->isMadeNonResident(globalFenceAllocation));

    flushTask(*commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(globalFenceAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(globalFenceAllocation));
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCommandStreamReceiverWhenFenceAllocationIsRequiredAndCreatedThenItIsMadeResidentDuringFlushSmallTask) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    MockCsrHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_EQ(nullptr, csr.globalFenceAllocation);
    EXPECT_TRUE(csr.createGlobalFenceAllocation());

    EXPECT_FALSE(csr.isMadeResident(csr.globalFenceAllocation));
    EXPECT_FALSE(csr.isMadeNonResident(csr.globalFenceAllocation));

    flushSmallTask(csr);

    EXPECT_TRUE(csr.isMadeResident(csr.globalFenceAllocation));
    EXPECT_TRUE(csr.isMadeNonResident(csr.globalFenceAllocation));

    ASSERT_NE(nullptr, csr.globalFenceAllocation);
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_FENCE, csr.globalFenceAllocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCommandStreamReceiverWhenFenceAllocationIsRequiredButNotCreatedThenItIsNotMadeResidentDuringFlushSmallTask) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    MockCsrHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_EQ(nullptr, csr.globalFenceAllocation);

    flushSmallTask(csr);

    ASSERT_EQ(nullptr, csr.globalFenceAllocation);
}

struct MockScratchController : public ScratchSpaceController {
    using ScratchSpaceController::privateScratchAllocation;
    using ScratchSpaceController::scratchAllocation;
    using ScratchSpaceController::ScratchSpaceController;
    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSize,
                                 uint32_t requiredPerThreadPrivateScratchSize,
                                 uint32_t currentTaskCount,
                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override {
        if (requiredPerThreadScratchSize > scratchSizeBytes) {
            scratchSizeBytes = requiredPerThreadScratchSize;
            scratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, requiredPerThreadScratchSize});
        }
        if (requiredPerThreadPrivateScratchSize > privateScratchSizeBytes) {
            privateScratchSizeBytes = requiredPerThreadPrivateScratchSize;
            privateScratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, requiredPerThreadPrivateScratchSize});
        }
    }
    uint64_t calculateNewGSH() override { return 0u; };
    uint64_t getScratchPatchAddress() override { return 0u; };
    void programHeaps(HeapContainer &heapContainer,
                      uint32_t scratchSlot,
                      uint32_t requiredPerThreadScratchSize,
                      uint32_t requiredPerThreadPrivateScratchSize,
                      uint32_t currentTaskCount,
                      OsContext &osContext,
                      bool &stateBaseAddressDirty,
                      bool &vfeStateDirty) override {
    }
    void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                               uint32_t requiredPerThreadScratchSize,
                                               uint32_t requiredPerThreadPrivateScratchSize,
                                               uint32_t currentTaskCount,
                                               OsContext &osContext,
                                               bool &stateBaseAddressDirty,
                                               bool &vfeStateDirty,
                                               NEO::CommandStreamReceiver *csr) override {
    }
    void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) override{};
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenScratchIsRequiredForFirstFlushAndPrivateScratchForSecondFlushThenHandleResidencyProperly) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = new MockScratchController(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *commandStreamReceiver->getInternalAllocationStorage());
    commandStreamReceiver->scratchSpaceController.reset(scratchController);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSizes(1024, 0);

    flushTask(*commandStreamReceiver);

    EXPECT_NE(nullptr, scratchController->scratchAllocation);
    EXPECT_EQ(nullptr, scratchController->privateScratchAllocation);

    auto scratchAllocation = scratchController->scratchAllocation;

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));

    commandStreamReceiver->madeResidentGfxAllocations.clear(); // this is only history - we can clean this
    commandStreamReceiver->madeNonResidentGfxAllocations.clear();
    commandStreamReceiver->setRequiredScratchSizes(0, 1024);

    flushTask(*commandStreamReceiver); // 2nd flush

    EXPECT_NE(nullptr, scratchController->scratchAllocation);
    EXPECT_NE(nullptr, scratchController->privateScratchAllocation);

    auto privateScratchAllocation = scratchController->privateScratchAllocation;

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(privateScratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(privateScratchAllocation));
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenPrivateScratchIsRequiredForFirstFlushAndCommonScratchForSecondFlushThenHandleResidencyProperly) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto scratchController = new MockScratchController(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *commandStreamReceiver->getInternalAllocationStorage());
    commandStreamReceiver->scratchSpaceController.reset(scratchController);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSizes(0, 1024);

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(nullptr, scratchController->scratchAllocation);
    EXPECT_NE(nullptr, scratchController->privateScratchAllocation);

    auto privateScratchAllocation = scratchController->privateScratchAllocation;

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(privateScratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(privateScratchAllocation));

    commandStreamReceiver->madeResidentGfxAllocations.clear(); // this is only history - we can clean this
    commandStreamReceiver->madeNonResidentGfxAllocations.clear();
    commandStreamReceiver->setRequiredScratchSizes(1024, 0);

    flushTask(*commandStreamReceiver); // 2nd flush

    EXPECT_NE(nullptr, scratchController->scratchAllocation);
    EXPECT_NE(nullptr, scratchController->privateScratchAllocation);

    auto scratchAllocation = scratchController->scratchAllocation;

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(privateScratchAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(privateScratchAllocation));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenTwoConsecutiveNdRangeKernelsThenStateBaseAddressIsProgrammedOnceAndScratchAddressInMediaVfeStateIsProgrammedTwiceBothWithCorrectAddress) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    kernel.kernelInfo.setPerThreadScratchSize(scratchSize, 0);

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    auto deviceInfo = pClDevice->getDeviceInfo();
    auto sharedDeviceInfo = pDevice->getDeviceInfo();
    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_TRUE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    // All state should be programmed before walker
    auto itorCmdForStateBase = itorStateBaseAddress;
    auto *mediaVfeState = (MEDIA_VFE_STATE *)*itorMediaVfeState;

    auto graphicsAllocationScratch = commandStreamReceiver->getScratchAllocation();

    ASSERT_NE(itorCmdForStateBase, itorWalker);
    auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
    auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();
    uint64_t graphicsAddress = 0;

    // Get address ( offset in 32 bit addressing ) of sratch
    graphicsAddress = (uint64_t)graphicsAllocationScratch->getGpuAddressToPatch();

    if (sharedDeviceInfo.force32BitAddressess && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation());
        EXPECT_EQ(GmmHelper::decanonize(graphicsAllocationScratch->getGpuAddress()) - GSHaddress, graphicsAddress);
    } else if (!deviceInfo.svmCapabilities && is64bit) {
        EXPECT_EQ(ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, mediaVfeState->getScratchSpaceBasePointer());
        EXPECT_EQ(GSHaddress + ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, graphicsAllocationScratch->getGpuAddressToPatch());
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !sharedDeviceInfo.force32BitAddressess) {
        uint64_t expectedAddress = ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
        EXPECT_EQ(expectedAddress, scratchBaseLowPart);
        EXPECT_EQ(0u, scratchBaseHighPart);
    } else {
        EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
        EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);
    }

    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_EQ(pDevice->getMemoryManager()->getExternalHeapBaseAddress(graphicsAllocationScratch->getRootDeviceIndex(), false), GSHaddress);
    } else {
        if constexpr (is64bit) {
            EXPECT_EQ(graphicsAddress - ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    //now re-try to see if SBA is not programmed
    scratchSize *= 2;
    kernel.kernelInfo.setPerThreadScratchSize(scratchSize, 0);

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    // In 32 Bit addressing sba shouldn't be reprogrammed
    if (sharedDeviceInfo.force32BitAddressess == true) {
        EXPECT_EQ(itorCmdForStateBase, cmdList.end());
    }

    auto itorMediaVfeStateSecond = find<MEDIA_VFE_STATE *>(itorWalker, cmdList.end());
    auto *cmdMediaVfeStateSecond = (MEDIA_VFE_STATE *)*itorMediaVfeStateSecond;

    EXPECT_NE(mediaVfeState, cmdMediaVfeStateSecond);

    uint64_t oldScratchAddr = ((uint64_t)scratchBaseHighPart << 32u) | scratchBaseLowPart;
    uint64_t newScratchAddr = ((uint64_t)cmdMediaVfeStateSecond->getScratchSpaceBasePointerHigh() << 32u) | cmdMediaVfeStateSecond->getScratchSpaceBasePointer();

    if (sharedDeviceInfo.force32BitAddressess == true) {
        EXPECT_NE(oldScratchAddr, newScratchAddr);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenNdRangeKernelAndReadBufferStateBaseAddressAndScratchAddressInMediaVfeStateThenProgrammingIsCorrect) {

    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    kernel.kernelInfo.setPerThreadScratchSize(scratchSize, 0);

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    auto deviceInfo = pClDevice->getDeviceInfo();
    auto sharedDeviceInfo = pDevice->getDeviceInfo();
    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_TRUE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    // All state should be programmed before walker
    auto itorCmdForStateBase = itorStateBaseAddress;
    auto *mediaVfeState = (MEDIA_VFE_STATE *)*itorMediaVfeState;

    auto graphicsAllocationScratch = commandStreamReceiver->getScratchAllocation();

    ASSERT_NE(itorCmdForStateBase, itorWalker);
    auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
    auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();
    uint64_t graphicsAddress = 0;

    // Get address ( offset in 32 bit addressing ) of sratch
    graphicsAddress = (uint64_t)graphicsAllocationScratch->getGpuAddressToPatch();

    if (sharedDeviceInfo.force32BitAddressess && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation());
        EXPECT_EQ(GmmHelper::decanonize(graphicsAllocationScratch->getGpuAddress()) - GSHaddress, graphicsAddress);
    } else if (!deviceInfo.svmCapabilities && is64bit) {
        EXPECT_EQ(ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, mediaVfeState->getScratchSpaceBasePointer());
        EXPECT_EQ(GSHaddress + ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, graphicsAllocationScratch->getGpuAddressToPatch());
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !sharedDeviceInfo.force32BitAddressess) {
        lowPartGraphicsAddress = ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
        highPartGraphicsAddress = 0u;
    }

    EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
    EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);

    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_EQ(pDevice->getMemoryManager()->getExternalHeapBaseAddress(graphicsAllocationScratch->getRootDeviceIndex(), false), GSHaddress);
    } else {
        if constexpr (is64bit) {
            EXPECT_EQ(graphicsAddress - ScratchSpaceConstants::scratchSpaceOffsetFor64Bit, GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 0, 0};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 0, 0, 0);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    if (sharedDeviceInfo.force32BitAddressess) {
        EXPECT_NE(itorWalker, itorCmdForStateBase);

        if (itorCmdForStateBase != cmdList.end()) {

            auto *sba2 = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
            auto GSHaddress2 = (uintptr_t)sba2->getGeneralStateBaseAddress();

            EXPECT_NE(sba, sba2);
            EXPECT_EQ(0u, GSHaddress2);

            if (sharedDeviceInfo.force32BitAddressess) {
                EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
            }
        }
    }
    delete buffer;

    if (sharedDeviceInfo.force32BitAddressess) {
        // Asserts placed after restoring old CSR to avoid heap corruption
        ASSERT_NE(itorCmdForStateBase, cmdList.end());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenForced32BitAllocationsModeStore32bitWhenFlushingTaskThenScratchAllocationIsNotReused) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    pDevice->getMemoryManager()->setForce32BitAllocations(true);

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSizes(4096, 0); // whatever > 0 (in page size)
    flushTask(*commandStreamReceiver);

    auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
    ASSERT_NE(scratchAllocation, nullptr);

    commandStreamReceiver->setRequiredScratchSizes(8196, 0); // whatever > first size

    flushTask(*commandStreamReceiver); // 2nd flush

    auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

    std::unique_ptr<GraphicsAllocation> allocationReusable = commandStreamReceiver->getInternalAllocationStorage()->obtainReusableAllocation(4096, GraphicsAllocation::AllocationType::LINEAR_STREAM);

    if (allocationReusable.get() != nullptr) {
        if constexpr (is64bit) {
            EXPECT_NE(scratchAllocation, allocationReusable.get());
        }
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationReusable.release());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenForced32BitAllocationsModeStore32bitWhenFlushingTaskThenScratchAllocationStoredOnTemporaryAllocationList) {
    if constexpr (is64bit) {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.Force32bitAddressing.set(true);

        auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        pDevice->resetCommandStreamReceiver(commandStreamReceiver);

        commandStreamReceiver->setRequiredScratchSizes(4096, 0); // whatever > 0 (in page size)
        flushTask(*commandStreamReceiver);

        auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
        ASSERT_NE(scratchAllocation, nullptr);

        commandStreamReceiver->setRequiredScratchSizes(8196, 0); // whatever > first size

        flushTask(*commandStreamReceiver); // 2nd flush

        auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
        EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

        CommandStreamReceiver *csrPtr = reinterpret_cast<CommandStreamReceiver *>(commandStreamReceiver);
        std::unique_ptr<GraphicsAllocation> allocationTemporary = commandStreamReceiver->getTemporaryAllocations().detachAllocation(0, nullptr, csrPtr, GraphicsAllocation::AllocationType::SCRATCH_SURFACE);

        EXPECT_EQ(scratchAllocation, allocationTemporary.get());
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationTemporary.release());
    }
}

HWTEST_F(UltCommandStreamReceiverTest, WhenFlushingAllCachesThenPipeControlIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getPipeControlFlushEnable());
    EXPECT_TRUE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
}

HWTEST_F(UltCommandStreamReceiverTest, givenDebugDisablingCacheFlushWhenAddingPipeControlWithCacheFlushThenOverrideRequestAndDisableCacheFlushFlags) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DoNotFlushCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args(true);
    args.constantCacheInvalidationEnable = true;
    args.instructionCacheInvalidateEnable = true;
    args.pipeControlFlushEnable = true;
    args.renderTargetCacheFlushEnable = true;
    args.stateCacheInvalidationEnable = true;
    args.textureCacheInvalidationEnable = true;
    args.vfCacheInvalidationEnable = true;

    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_FALSE(pipeControl->getDcFlushEnable());
    EXPECT_FALSE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getPipeControlFlushEnable());
    EXPECT_FALSE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getStateCacheInvalidationEnable());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenEnabledPreemptionWhenFlushTaskCalledThenDontProgramMediaVfeStateAgain) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    HardwareParse hwParser;

    flushTask(csr, false, 0);
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);

    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_NE(nullptr, cmd);

    // program again
    csr.setMediaVFEStateDirty(false);
    auto offset = csr.commandStream.getUsed();
    flushTask(csr, false, commandStream.getUsed());
    hwParser.cmdList.clear();
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

    cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_EQ(nullptr, cmd);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, GivenPreambleSentAndL3ConfigChangedWhenFlushingTaskThenPipeControlIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    CsrSizeRequestFlags csrSizeRequest = {};

    commandStream.getSpace(sizeof(PIPE_CONTROL));
    flushTaskFlags.useSLM = true;
    flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    commandStreamReceiver.lastSentCoherencyRequest = false;
    commandStreamReceiver.lastSentThreadArbitrationPolicy = commandStreamReceiver.requiredThreadArbitrationPolicy;
    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto expectedUsed = csrCS.getUsed() + sizeNeeded;
    expectedUsed = alignUp(expectedUsed, MemoryConstants::cacheLineSize);

    commandStreamReceiver.flushTask(commandStream, 0, dsh, ioh, ssh, taskLevel, flushTaskFlags, *pDevice);

    // Verify that we didn't grab a new CS buffer
    EXPECT_EQ(expectedUsed, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnL3ConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);
    auto expectedDifference = commandStreamReceiver.getCmdSizeForL3Config();

    csrSizeRequest.l3ConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto difference = l3ConfigChangedSize - l3ConfigNotChangedSize;
    EXPECT_EQ(expectedDifference, difference);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleNotSentThenRequiredCsrSizeDoesntDependOnL3ConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    commandStreamReceiver.isPreambleSent = false;

    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    csrSizeRequest.l3ConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    EXPECT_EQ(l3ConfigNotChangedSize, l3ConfigChangedSize);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleNotSentThenRequiredCsrSizeDoesntDependOnmediaSamplerConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    commandStreamReceiver.isPreambleSent = false;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_EQ(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenSpecialPipelineSelectModeChangedWhenGetCmdSizeForPielineSelectIsCalledThenCorrectSizeIsReturned) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();

    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csrSizeRequest.specialPipelineSelectModeChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    size_t size = commandStreamReceiver.getCmdSizeForPipelineSelect();

    size_t expectedSize = sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<FamilyType>::isPipeControlPriorToPipelineSelectWArequired(pDevice->getHardwareInfo())) {
        expectedSize += sizeof(PIPE_CONTROL);
    }
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnmediaSamplerConfigChanged) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_NE(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
    auto difference = mediaSamplerConfigChangedSize - mediaSamplerConfigNotChangedSize;

    size_t expectedDifference = sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<FamilyType>::isPipeControlPriorToPipelineSelectWArequired(pDevice->getHardwareInfo())) {
        expectedDifference += sizeof(PIPE_CONTROL);
    }

    EXPECT_EQ(expectedDifference, difference);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenSamplerCacheFlushSentThenRequiredCsrSizeContainsPipecontrolSize) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getGpgpuCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    commandStreamReceiver.isPreambleSent = true;

    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    auto samplerCacheNotFlushedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    auto samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    EXPECT_EQ(samplerCacheNotFlushedSize, samplerCacheFlushBeforeSize);

    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    auto difference = samplerCacheFlushBeforeSize - samplerCacheNotFlushedSize;
    EXPECT_EQ(sizeof(typename FamilyType::PIPE_CONTROL), difference);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>(false);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(false);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    //surfaces are non resident
    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenCsrWhenGeneralStateBaseAddressIsProgrammedThenDecanonizedAddressIsWritten) {
    uint64_t generalStateBaseAddress = 0xffff800400010000ull;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                &dsh,
                                                                &ioh,
                                                                &ssh,
                                                                generalStateBaseAddress,
                                                                true,
                                                                0,
                                                                0,
                                                                generalStateBaseAddress,
                                                                0,
                                                                true,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_NE(generalStateBaseAddress, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(GmmHelper::decanonize(generalStateBaseAddress), sbaCmd.getGeneralStateBaseAddress());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNonZeroGeneralStateBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    uint64_t generalStateBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                &dsh,
                                                                &ioh,
                                                                &ssh,
                                                                generalStateBaseAddress,
                                                                false,
                                                                0,
                                                                0,
                                                                generalStateBaseAddress,
                                                                0,
                                                                true,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_EQ(0ull, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getGeneralStateBufferSize());
    EXPECT_FALSE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNonZeroInternalHeapBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    uint64_t internalHeapBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                &dsh,
                                                                &ioh,
                                                                &ssh,
                                                                internalHeapBaseAddress,
                                                                true,
                                                                0,
                                                                internalHeapBaseAddress,
                                                                0,
                                                                0,
                                                                false,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_FALSE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(0ull, sbaCmd.getInstructionBaseAddress());
    EXPECT_FALSE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getInstructionBufferSize());
    EXPECT_EQ(0u, sbaCmd.getInstructionMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenSbaProgrammingWhenHeapsAreNotProvidedThenDontProgram) {
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    uint64_t internalHeapBase = 0x10000;
    uint64_t instructionHeapBase = 0x10000;
    uint64_t generalStateBase = 0x30000;
    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                nullptr,
                                                                nullptr,
                                                                nullptr,
                                                                generalStateBase,
                                                                true,
                                                                0,
                                                                internalHeapBase,
                                                                instructionHeapBase,
                                                                0,
                                                                true,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getIndirectObjectBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(GmmHelper::decanonize(generalStateBase), sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());
}
