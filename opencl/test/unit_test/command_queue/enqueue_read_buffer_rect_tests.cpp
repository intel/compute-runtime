/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/memory_manager/memory_constants.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/enqueue_read_buffer_rect_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_enqueue_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "test.h"

#include "reg_configs_common.h"

using namespace NEO;

HWTEST_F(EnqueueReadBufferRectTest, GivenNullBufferWhenReadingBufferThenInvalidMemObjectErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 0};

    retVal = clEnqueueReadBufferRect(
        pCmdQ,
        nullptr,
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        10,
        0,
        10,
        0,
        hostPtr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueReadBufferRectTest, GivenNullHostPtrWhenReadingBufferThenInvalidValueErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 0};

    retVal = clEnqueueReadBufferRect(
        pCmdQ,
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        10,
        0,
        10,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(EnqueueReadBufferRectTest, GivenValidParamsWhenReadingBufferThenSuccessIsReturned) {
    auto retVal = CL_SUCCESS;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};

    retVal = clEnqueueReadBufferRect(
        pCmdQ,
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        10,
        0,
        10,
        0,
        hostPtr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueReadBufferRectTest, GivenBlockingEnqueueWhenReadingBufferThenTaskLevelIsNotIncremented) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    enqueueReadBufferRect2D<FamilyType>(CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueReadBufferRectTest, GivenNonBlockingEnqueueWhenReadingBufferThenTaskLevelIsIncremented) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueReadBufferRect2D<FamilyType>(CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, Given2dRegionWhenReadingBufferThenCommandsAreProgrammedCorrectly) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueReadBufferRect2D<FamilyType>();

    ASSERT_NE(cmdList.end(), itorWalker);
    auto *cmd = (GPGPU_WALKER *)*itorWalker;

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_EQ(1u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueReadBufferRectTest, WhenReadingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueReadBufferRect2D<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueReadBufferRectTest, WhenReadingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueReadBufferRect2D<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, WhenReadingBufferThenIndirectDataIsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueReadBufferRect2D<FamilyType>();

    // Extract the kernel used
    MultiDispatchInfo multiDispatchInfo;
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferRect,
                                                                            pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = buffer.get();
    dc.dstPtr = hostPtr;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {50, 50, 1};
    dc.srcRowPitch = rowPitch;
    dc.srcSlicePitch = slicePitch;
    dc.dstRowPitch = rowPitch;
    dc.dstSlicePitch = slicePitch;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    EXPECT_NE(dshBefore, pDSH->getUsed());
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->requiresSshForBuffers()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueReadBufferRectTest, WhenReadingBufferThenL3ProgrammingIsCorrect) {
    enqueueReadBufferRect2D<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, When2DEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueReadBufferRect2D<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, WhenReadingBufferThenMediaInterfaceDescriptorIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadBufferRect2D<FamilyType>();

    // All state should be programmed before walker
    auto itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(itorPipelineSelect, itorWalker);
    ASSERT_NE(itorWalker, itorCmd);

    auto *cmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorCmd;

    // Verify we have a valid length -- multiple of INTERFACE_DESCRIPTOR_DATAs
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % sizeof(INTERFACE_DESCRIPTOR_DATA));

    // Validate the start address
    size_t alignmentStartAddress = 64 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorDataStartAddress() % alignmentStartAddress);

    // Validate the length
    EXPECT_NE(0u, cmd->getInterfaceDescriptorTotalLength());
    size_t alignmentTotalLength = 32 * sizeof(uint8_t);
    EXPECT_EQ(0u, cmd->getInterfaceDescriptorTotalLength() % alignmentTotalLength);

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorCmd);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, WhenReadingBufferThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueReadBufferRect2D<FamilyType>();

    // Extract the MIDL command
    auto itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(itorPipelineSelect, itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdMIDL = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorCmd;

    // Extract the SBA command
    itorCmd = find<STATE_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdSBA = (STATE_BASE_ADDRESS *)*itorCmd;

    // Extrach the DSH
    auto DSH = cmdSBA->getDynamicStateBaseAddress();
    ASSERT_NE(0u, DSH);

    // IDD should be located within DSH
    auto iddStart = cmdMIDL->getInterfaceDescriptorDataStartAddress();
    auto IDDEnd = iddStart + cmdMIDL->getInterfaceDescriptorTotalLength();
    ASSERT_LE(IDDEnd, cmdSBA->getDynamicStateBufferSize() * MemoryConstants::pageSize);

    auto &IDD = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)IDD.getKernelStartPointerHigh() << 32) + IDD.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, IDD.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, IDD.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, IDD.getConstantIndirectUrbEntryReadLength());
}

HWTEST_F(EnqueueReadBufferRectTest, WhenReadingBufferThenOnePipelineSelectIsProgrammed) {
    enqueueReadBufferRect2D<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, WhenReadingBufferThenMediaVfeStateIsCorrect) {
    enqueueReadBufferRect2D<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueReadBufferRectTest, GivenBlockingEnqueueWhenReadingBufferThenPipeControlIsProgrammedAfterWalkerWithDcFlushSet) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto blocking = CL_TRUE;
    enqueueReadBufferRect2D<FamilyType>(blocking);

    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());

    // All state should be programmed after walker
    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    auto *cmd = (PIPE_CONTROL *)*itorCmd;
    EXPECT_NE(cmdList.end(), itorCmd);

    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        // SKL: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_FALSE(cmd->getDcFlushEnable());
        // Move to next PPC
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmd = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_TRUE(cmd->getDcFlushEnable());
    } else {
        // single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {50, 50, 1};
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        ptr,
        numEventsInWaitList,
        eventWaitList,
        &event);
    ;

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdQ->taskLevel);
    EXPECT_EQ(CL_COMMAND_READ_BUFFER_RECT, (const int)pEvent->getCommandType());

    pEvent->release();
}
HWTEST_F(EnqueueReadBufferRectTest, givenOutOfOrderQueueAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {50, 50, 1};
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdOOQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        ptr,
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdOOQ->taskLevel);
    EXPECT_EQ(CL_COMMAND_READ_BUFFER_RECT, (const int)pEvent->getCommandType());

    pEvent->release();
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndRowPitchEqualZeroAndDstPtrEqualSrcPtrWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {50, 50, 1};
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        0,
        slicePitch,
        0,
        slicePitch,
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndSlicePitchEqualZeroAndDstPtrEqualSrcPtrWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {50, 50, 1};
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        0,
        rowPitch,
        0,
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndMemObjWithOffsetPointTheSameStorageWithHostWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    size_t bufferOrigin[] = {50, 50, 0};
    size_t hostOrigin[] = {20, 20, 0};
    size_t region[] = {50, 50, 1};
    size_t hostOffset = (bufferOrigin[2] - hostOrigin[2]) * slicePitch + (bufferOrigin[1] - hostOrigin[1]) * rowPitch + (bufferOrigin[0] - hostOrigin[0]);
    auto hostStorage = ptrOffset(ptr, hostOffset);
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        hostStorage,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndMemObjWithOffsetPointDiffrentStorageWithHostWhenReadBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    void *ptr = buffer->getCpuAddressForMemoryTransfer();
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {1, 1, 0};
    size_t region[] = {1, 1, 1};
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}
HWTEST_F(EnqueueReadBufferRectTest, givenInOrderQueueAndDstPtrEqualSrcPtrAndNonZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    cl_int retVal = CL_SUCCESS;
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {1, 1, 1};
    retVal = pCmdQ->enqueueReadBufferRect(
        nonZeroCopyBuffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        ptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}

HWTEST_F(EnqueueReadWriteBufferRectDispatch, givenOffsetResultingInMisalignedPtrWhenEnqueueReadBufferRectForNon3DCaseIsCalledThenAddressInStateBaseAddressIsAlignedAndMatchesKernelDispatchInfoParams) {
    initializeFixture<FamilyType>();
    if (device->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), &properties);

    buffer->forceDisallowCPUCopy = true;
    Vec3<size_t> hostOffset(hostOrigin);
    auto misalignedDstPtr = ptrOffset(reinterpret_cast<void *>(memory), hostOffset.z * hostSlicePitch);

    auto retVal = cmdQ->enqueueReadBufferRect(buffer.get(), CL_FALSE,
                                              bufferOrigin, hostOrigin, region,
                                              bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch,
                                              memory, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, cmdQ->lastEnqueuedKernels.size());
    Kernel *kernel = cmdQ->lastEnqueuedKernels[0];

    cmdQ->finish();

    parseCommands<FamilyType>(*cmdQ);

    if (hwInfo->capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress) {
        const auto &surfaceStateDst = getSurfaceState<FamilyType>(&cmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0), 1);

        if (kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].size == sizeof(uint64_t)) {
            auto pKernelArg = (uint64_t *)(kernel->getCrossThreadData() +
                                           kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);
            EXPECT_EQ(reinterpret_cast<uint64_t>(alignDown(misalignedDstPtr, 4)), *pKernelArg);
            EXPECT_EQ(*pKernelArg, surfaceStateDst.getSurfaceBaseAddress());

        } else if (kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].size == sizeof(uint32_t)) {
            auto pKernelArg = (uint32_t *)(kernel->getCrossThreadData() +
                                           kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);
            EXPECT_EQ(reinterpret_cast<uint64_t>(alignDown(misalignedDstPtr, 4)), static_cast<uint64_t>(*pKernelArg));
            EXPECT_EQ(static_cast<uint64_t>(*pKernelArg), surfaceStateDst.getSurfaceBaseAddress());
        }
    }

    if (kernel->getKernelInfo().kernelArgInfo[3].kernelArgPatchInfoVector[0].size == 4 * sizeof(uint32_t)) { // size of  uint4 DstOrigin
        auto dstOffset = (uint32_t *)(kernel->getCrossThreadData() +
                                      kernel->getKernelInfo().kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(hostOffset.x + ptrDiff(misalignedDstPtr, alignDown(misalignedDstPtr, 4)), *dstOffset);
    } else {
        // DstOrigin arg should be 16 bytes in size, if that changes, above if path should be modified
        EXPECT_TRUE(false);
    }
}

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueReadBufferRectWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    constexpr size_t rowPitch = 100;
    constexpr size_t slicePitch = 100 * 100;
    size_t bufferOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {50, 50, 1};
    retVal = pCmdQ->enqueueReadBufferRect(
        buffer.get(),
        CL_FALSE,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        ptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

struct EnqueueReadBufferRectHw : public ::testing::Test {

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    MockBuffer srcBuffer;

    size_t bufferOrigin[3] = {0, 0, 0};
    size_t hostOrigin[3] = {0, 0, 0};
    size_t region[3] = {1, 1, 1};
    size_t bufferRowPitch = 10;
    size_t bufferSlicePitch = 0;
    size_t hostRowPitch = 10;
    size_t hostSlicePitch = 10;
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
};

using EnqeueReadBufferRectStatelessTest = EnqueueReadBufferRectHw;

HWTEST_F(EnqeueReadBufferRectStatelessTest, WhenReadingBufferRectStatelessThenSuccessIsReturned) {

    auto pCmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = pCmdQ->enqueueReadBufferRect(&srcBuffer,
                                               CL_FALSE,
                                               bufferOrigin,
                                               hostOrigin,
                                               region,
                                               bufferRowPitch,
                                               bufferSlicePitch,
                                               hostRowPitch,
                                               hostSlicePitch,
                                               missAlignedPtr,
                                               0,
                                               nullptr,
                                               nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

using EnqeueReadBufferRectStatefulTest = EnqueueReadBufferRectHw;

HWTEST_F(EnqeueReadBufferRectStatefulTest, WhenReadingBufferRectStatefulThenSuccessIsReturned) {

    auto pCmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = pCmdQ->enqueueReadBufferRect(&srcBuffer,
                                               CL_FALSE,
                                               bufferOrigin,
                                               hostOrigin,
                                               region,
                                               bufferRowPitch,
                                               bufferSlicePitch,
                                               hostRowPitch,
                                               hostSlicePitch,
                                               missAlignedPtr,
                                               0,
                                               nullptr,
                                               nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}
