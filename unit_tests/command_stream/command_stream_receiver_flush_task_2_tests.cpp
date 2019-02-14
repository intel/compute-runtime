/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "reg_configs_common.h"
#include "runtime/command_stream/csr_definitions.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/state_base_address.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

using namespace OCLRT;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskTests;

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockedKernelNotRequiringDCFlushWhenUnblockedThenDCFlushIsNotAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 1, 2};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    auto &commandStreamCSR = commandStreamReceiver.getCS();
    auto &commandStreamTask = commandQueue.getCS(1024);

    commandQueue.enqueueWriteBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 1, &blockingEvent, 0);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);

    // Verify that the dcFlushEnabled bit is set in PC
    auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pCmdWA->getDcFlushEnable());

    buffer->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, FlushTaskWithTaskCSPassedAsCommandStreamParam) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &commandStreamTask = commandQueue.getCS(1024);

    DispatchFlags dispatchFlags;

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

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenEmptyQueue) {
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    commandQueue.finish(false);
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    commandQueue.finish(true);
    //nothings sent to the HW, no need to bump tags
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(0u, commandQueue.latestTaskCountWaited);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenNonDcFlushWithInitialTaskCountZero) {
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish after enqueued kernel(cmdq task count = 1)
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    commandQueue.finish(false);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    // finish again - dont call flush task
    commandQueue.finish(false);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenDcFlush) {
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;
    size_t tempBuffer[] = {0, 1, 2};
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

    // finish(dcFlush=true) from blocking MapBuffer after enqueued kernel
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    auto ptr = commandQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    // cmdQ task count = 2, finish again
    commandQueue.finish(false);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);

    // finish again - dont flush task again
    commandQueue.finish(false);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);

    // finish(dcFlush=true) from MapBuffer again - dont call FinishTask n finished queue
    retVal = commandQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ptr = commandQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    //cleanup
    retVal = commandQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenPowerOfTwoGlobalWorkSizeAndNullLocalWorkgroupSizeWhenEnqueueKernelIsCalledThenGpGpuWalkerHasOptimalSIMDmask) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    size_t GWS = 1024;
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenEventIsQueried) {
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
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

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    pEvent = (Event *)event;

    retVal = Event::waitForEvents(1, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    retVal = clReleaseEvent(pEvent);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenNonBlockingMapWhenFinishIsCalledThenNothingIsSubmittedToTheHardware) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
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

    commandQueue.finish(false);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    auto &commandStreamTask = commandQueue.getCS(1024);
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests,
            GivenFlushedCallRequiringDCFlushWhenBlockingEnqueueIsCalledThenPipeControlWithDCFlushIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    cl_event event = nullptr;

    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto &commandStreamTask = commandQueue.getCS(1024);

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {5, 5, 5};

    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // Call requiring DCFlush, nonblocking
    buffer->forceDisallowCPUCopy = true;
    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, 0);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, &event);

    EXPECT_EQ(2u, commandStreamReceiver.peekLatestSentTaskCount());

    EXPECT_EQ(2u, commandQueue.latestTaskCountWaited);

    // Parse command list to verify that PC was added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorWalker = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorCmd);

    auto cmdPC = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, cmdPC);

    if (::renderCoreFamily != IGFX_GEN8_CORE) {
        // SKL+: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_TRUE(cmdPC->getDcFlushEnable());
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmdPC = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_FALSE(cmdPC->getDcFlushEnable());
    } else {
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmdPC->getDcFlushEnable());
    }

    retVal = clReleaseEvent(event);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDefaultCommandStreamReceiverThenRoundRobinPolicyIsSelected) {
    MockCsrHw<FamilyType> commandStreamReceiver(*platformDevices[0], *pDevice->executionEnvironment);
    EXPECT_EQ(PreambleHelper<FamilyType>::getDefaultThreadArbitrationPolicy(), commandStreamReceiver.peekThreadArbitrationPolicy());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenKernelWithSlmWhenPreviousSLML3WasSentThenDontProgramL3) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t GWS = 1;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    uint32_t L3Config = PreambleHelper<FamilyType>::getL3Config(*platformDevices[0], true);

    // Mark Pramble as sent, override L3Config to SLM config
    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = L3Config;
    commandStreamReceiver->lastSentThreadArbitrationPolicy = kernel.mockKernel->getThreadArbitrationPolicy<FamilyType>();

    ((MockKernel *)kernel)->setTotalSLMSize(1024);

    cmdList.clear();
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list to verify that PC was added to taskCS
    parseCommands<FamilyType>(commandStreamCSR, 0);

    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<FamilyType>::registerOffset);
    EXPECT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, CreateCommandStreamReceiverHw) {
    const HardwareInfo hwInfo = *platformDevices[0];
    auto csrHw = CommandStreamReceiverHw<FamilyType>::create(hwInfo, executionEnvironment);
    EXPECT_NE(nullptr, csrHw);

    MemoryManager *mm = csrHw->createMemoryManager(false, false);
    EXPECT_EQ(nullptr, mm);
    GmmPageTableMngr *ptm = csrHw->createPageTableManager();
    EXPECT_EQ(nullptr, ptm);
    delete csrHw;

    DebugManager.flags.SetCommandStreamReceiver.set(0);
    int32_t GetCsr = DebugManager.flags.SetCommandStreamReceiver.get();
    EXPECT_EQ(0, GetCsr);

    ExecutionEnvironment executionEnvironment;
    auto csr = OCLRT::createCommandStream(&hwInfo, executionEnvironment);
    EXPECT_NE(nullptr, csr);
    delete csr;
    DebugManager.flags.SetCommandStreamReceiver.set(0);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, handleTagAndScratchAllocationsResidencyOnEachFlush) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSize(1024); // whatever > 0

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

    auto NewScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_EQ(scratchAllocation, NewScratchAllocation); // Allocation unchanged. Dont skip residency handling

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));

    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenTwoConsecutiveNDRangeKernelsStateBaseAddressIsProgrammedOnceAndScratchAddressInMediaVFEStateIsProgrammedTwiceBothWithCorrectAddress) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;
    kernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
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

    if (pDevice->getDeviceInfo().force32BitAddressess == true && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation);
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getGpuAddress() - GSHaddress, graphicsAddress);
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !pDevice->getDeviceInfo().force32BitAddressess) {
        uint64_t expectedAddress = HwHelperHw<FamilyType>::get().getScratchSpaceOffsetFor64bit();
        EXPECT_EQ(expectedAddress, scratchBaseLowPart);
        EXPECT_EQ(0u, scratchBaseHighPart);
    } else {
        EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
        EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);
    }

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(pDevice->getMemoryManager()->allocator32Bit->getBase(), GSHaddress);
    } else {
        if (is64bit) {
            EXPECT_EQ(graphicsAddress - HwHelperHw<FamilyType>::get().getScratchSpaceOffsetFor64bit(), GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    //now re-try to see if SBA is not programmed
    scratchSize *= 2;
    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    // In 32 Bit addressing sba shouldn't be reprogrammed
    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(itorCmdForStateBase, cmdList.end());
    }

    auto itorMediaVfeStateSecond = find<MEDIA_VFE_STATE *>(itorWalker, cmdList.end());
    auto *cmdMediaVfeStateSecond = (MEDIA_VFE_STATE *)*itorMediaVfeStateSecond;

    EXPECT_NE(mediaVfeState, cmdMediaVfeStateSecond);

    uint64_t oldScratchAddr = ((uint64_t)scratchBaseHighPart << 32u) | scratchBaseLowPart;
    uint64_t newScratchAddr = ((uint64_t)cmdMediaVfeStateSecond->getScratchSpaceBasePointerHigh() << 32u) | cmdMediaVfeStateSecond->getScratchSpaceBasePointer();

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_NE(oldScratchAddr, newScratchAddr);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenNDRangeKernelAndReadBufferStateBaseAddressAndScratchAddressInMediaVFEStateIsProgrammedForNDRangeAndReprogramedForReadBufferAndGSBAFlagIsResetToFalse) {

    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;
    kernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
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

    if (pDevice->getDeviceInfo().force32BitAddressess == true && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation);
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getGpuAddress() - GSHaddress, graphicsAddress);
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !pDevice->getDeviceInfo().force32BitAddressess) {
        lowPartGraphicsAddress = HwHelperHw<FamilyType>::get().getScratchSpaceOffsetFor64bit();
        highPartGraphicsAddress = 0u;
    }

    EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
    EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(pDevice->getMemoryManager()->allocator32Bit->getBase(), GSHaddress);
    } else {
        if (is64bit) {
            EXPECT_EQ(graphicsAddress - HwHelperHw<FamilyType>::get().getScratchSpaceOffsetFor64bit(), GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 0, 0};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, 0);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_NE(itorWalker, itorCmdForStateBase);

        if (itorCmdForStateBase != cmdList.end()) {

            auto *sba2 = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
            auto GSHaddress2 = (uintptr_t)sba2->getGeneralStateBaseAddress();

            EXPECT_NE(sba, sba2);
            EXPECT_EQ(0u, GSHaddress2);

            if (pDevice->getDeviceInfo().force32BitAddressess == true) {
                EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
            }
        }
    }
    delete buffer;

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        // Asserts placed after restoring old CSR to avoid heap corruption
        ASSERT_NE(itorCmdForStateBase, cmdList.end());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, InForced32BitAllocationsModeDoNotStore32bitScratchAllocationOnReusableAllocationList) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);

    pDevice->getMemoryManager()->setForce32BitAllocations(true);

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSize(4096); // whatever > 0 (in page size)
    flushTask(*commandStreamReceiver);

    auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
    ASSERT_NE(scratchAllocation, nullptr);

    commandStreamReceiver->setRequiredScratchSize(8196); // whatever > first size

    flushTask(*commandStreamReceiver); // 2nd flush

    auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

    std::unique_ptr<GraphicsAllocation> allocationReusable = commandStreamReceiver->getInternalAllocationStorage()->obtainReusableAllocation(4096, GraphicsAllocation::AllocationType::LINEAR_STREAM);

    if (allocationReusable.get() != nullptr) {
        if (is64bit) {
            EXPECT_NE(scratchAllocation, allocationReusable.get());
        }
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationReusable.release());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, InForced32BitAllocationsModeStore32bitScratchAllocationOnTemporaryAllocationList) {
    if (is64bit) {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.Force32bitAddressing.set(true);

        auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);

        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        pDevice->resetCommandStreamReceiver(commandStreamReceiver);

        commandStreamReceiver->setRequiredScratchSize(4096); // whatever > 0 (in page size)
        flushTask(*commandStreamReceiver);

        auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
        ASSERT_NE(scratchAllocation, nullptr);

        commandStreamReceiver->setRequiredScratchSize(8196); // whatever > first size

        flushTask(*commandStreamReceiver); // 2nd flush

        auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
        EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

        std::unique_ptr<GraphicsAllocation> allocationTemporary = commandStreamReceiver->getTemporaryAllocations().detachAllocation(0, *commandStreamReceiver, GraphicsAllocation::AllocationType::SCRATCH_SURFACE);

        EXPECT_EQ(scratchAllocation, allocationTemporary.get());
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationTemporary.release());
    }
}

TEST(CacheSettings, GivenCacheSettingWhenCheckedForValuesThenProperValuesAreSelected) {
    EXPECT_EQ(static_cast<uint32_t>(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), CacheSettings::l3CacheOff);
    EXPECT_EQ(static_cast<uint32_t>(GMM_RESOURCE_USAGE_OCL_BUFFER), CacheSettings::l3CacheOn);
}

HWTEST_F(UltCommandStreamReceiverTest, addPipeControlWithFlushAllCaches) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    auto &csr = pDevice->getCommandStreamReceiver();

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    csr.addPipeControl(stream, false);

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

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, flushTaskWithPCWhenPreambleSentAndL3ConfigChanged) {
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
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
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
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
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
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = false;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_EQ(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnmediaSamplerConfigChanged) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_NE(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
    auto difference = mediaSamplerConfigChangedSize - mediaSamplerConfigNotChangedSize;
    EXPECT_EQ(sizeof(PIPELINE_SELECT), difference);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenSamplerCacheFlushSentThenRequiredCsrSizeContainsPipecontrolSize) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = true;

    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    auto samplerCacheNotFlushedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    auto samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    EXPECT_EQ(samplerCacheNotFlushedSize, samplerCacheFlushBeforeSize);

    OCLRT::WorkaroundTable *waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());
    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    auto difference = samplerCacheFlushBeforeSize - samplerCacheNotFlushedSize;
    EXPECT_EQ(sizeof(typename FamilyType::PIPE_CONTROL), difference);
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenGeneralStateBaseAddressIsProgrammedThenDecanonizedAddressIsWritten) {
    uint64_t generalStateBaseAddress = 0xffff800400010000ull;
    StateBaseAddressHelper<FamilyType> helper;
    DispatchFlags dispatchFlags;

    helper.programStateBaseAddress(commandStream,
                                   dsh,
                                   ioh,
                                   ssh,
                                   generalStateBaseAddress,
                                   0,
                                   generalStateBaseAddress,
                                   pDevice->getGmmHelper(),
                                   dispatchFlags);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream);
    auto cmd = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();

    EXPECT_NE(generalStateBaseAddress, cmd->getGeneralStateBaseAddress());
    EXPECT_EQ(GmmHelper::decanonize(generalStateBaseAddress), cmd->getGeneralStateBaseAddress());
}
