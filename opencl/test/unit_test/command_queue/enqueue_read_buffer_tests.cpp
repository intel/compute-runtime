/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_read_buffer_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

HWTEST_F(EnqueueReadBufferTypeTest, GivenNullBufferWhenReadingBufferThenInvalidMemObjectErrorIsReturned) {
    auto data = 1;
    auto retVal = clEnqueueReadBuffer(
        pCmdQ,
        nullptr,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueReadBufferTypeTest, GivenNullUserPointerWhenReadingBufferThenInvalidValueErrorIsReturned) {
    auto data = 1;

    auto retVal = clEnqueueReadBuffer(
        pCmdQ,
        srcBuffer.get(),
        false,
        0,
        sizeof(data),
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, WhenReadingBufferThenGpgpuWalkerIsProgrammedCorrectly) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();

    ASSERT_NE(cmdList.end(), itorWalker);
    auto *cmd = (GPGPU_WALKER *)*itorWalker;

    // Verify GPGPU_WALKER parameters
    EXPECT_NE(0u, cmd->getThreadGroupIdXDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdYDimension());
    EXPECT_NE(0u, cmd->getThreadGroupIdZDimension());
    EXPECT_NE(0u, cmd->getRightExecutionMask());
    EXPECT_NE(0u, cmd->getBottomExecutionMask());
    EXPECT_EQ(GPGPU_WALKER::SIMD_SIZE_SIMD32, cmd->getSimdSize());
    EXPECT_NE(0u, cmd->getIndirectDataLength());
    EXPECT_FALSE(cmd->getIndirectParameterEnable());

    // Compute the SIMD lane mask
    size_t simd =
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16
                                                                                                                         : 8;
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueReadBufferTypeTest, WhenReadingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueReadBufferTypeTest, GivenGpuHangAndBlockingCallWhenReadingBufferThenOutOfResourcesIsReturned) {
    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props{};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    srcBuffer->forceDisallowCPUCopy = true;
    const auto enqueueResult = EnqueueReadBufferHelper<>::enqueueReadBuffer(&mockCommandQueueHw, srcBuffer.get(), CL_TRUE);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueReadBufferTypeTest, GivenBlockingWhenReadingBufferThenAlignedToCsr) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueReadBufferTypeTest, GivenNonBlockingWhenReadingBufferThenAlignedToCsr) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, srcBuffer.get(), CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueReadBufferTypeTest, WhenReadingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueReadBufferTypeTest, WhenReadingBufferThenIndirectDataIsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);

    auto builtInType = pCmdQ->getHeaplessModeEnabled() ? EBuiltInOps::copyBufferToBufferStatelessHeapless : EBuiltInOps::copyBufferToBuffer;
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.dstPtr = EnqueueReadBufferTraits::hostPtr;
    dc.srcMemObj = srcBuffer.get();
    dc.srcOffset = {EnqueueReadBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    auto kernelDescriptor = &kernel->getKernelInfo().kernelDescriptor;

    auto crossThreadDatSize = kernel->getCrossThreadDataSize();
    auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(pCmdQ->getHeaplessModeEnabled());
    bool crossThreadDataFitsInInlineData = (crossThreadDatSize <= inlineDataSize);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernelDescriptor, rootDeviceIndex));

    if (crossThreadDataFitsInInlineData) {
        EXPECT_EQ(iohBefore, pIOH->getUsed());
    } else {
        EXPECT_NE(iohBefore, pIOH->getUsed());
    }

    if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueReadBufferTypeTest, WhenReadingBufferThenLoadRegisterImmediateL3CntlregIsCorrect) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, WhenReadingBufferThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();

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
    FamilyType::Parse::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorCmd);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, WhenReadingBufferThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();

    // Extract the MIDL command
    auto itorCmd = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(itorPipelineSelect, itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdMIDL = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorCmd;

    // Extract the SBA command
    itorCmd = find<STATE_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    ASSERT_NE(itorWalker, itorCmd);
    auto *cmdSBA = (STATE_BASE_ADDRESS *)*itorCmd;

    // Extrach the DSH
    auto dsh = cmdSBA->getDynamicStateBaseAddress();
    ASSERT_NE(0u, dsh);

    // IDD should be located within DSH
    auto iddStart = cmdMIDL->getInterfaceDescriptorDataStartAddress();
    auto iddEnd = iddStart + cmdMIDL->getInterfaceDescriptorTotalLength();
    ASSERT_LE(iddEnd, cmdSBA->getDynamicStateBufferSize() * MemoryConstants::pageSize);

    auto &idd = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)idd.getKernelStartPointerHigh() << 32) + idd.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, idd.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, idd.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, idd.getConstantIndirectUrbEntryReadLength());
}

HWTEST2_F(EnqueueReadBufferTypeTest, WhenReadingBufferThenPipelineSelectIsProgrammedOnce, IsAtMostXeCore) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, WhenReadingBufferThenMediaVfeStateIsCorrect) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueReadBufferTypeTest, GivenBlockingWhenReadingBufferThenPipeControlAfterWalkerWithDcFlushSetIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueReadBuffer<FamilyType>(CL_TRUE);

    // All state should be programmed after walker
    auto itorWalker = find<typename FamilyType::GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
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
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

HWTEST_F(EnqueueReadBufferTypeTest, givenAlignedPointerAndAlignedSizeWhenReadBufferIsCalledThenRecordedL3IndexIsL3OrL1ON) {

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    void *ptr = (void *)0x1040;

    cl_int retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                             CL_FALSE,
                                             0,
                                             MemoryConstants::cacheLineSize,
                                             ptr,
                                             nullptr,
                                             0,
                                             nullptr,
                                             nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto gmmHelper = pDevice->getGmmHelper();
    auto mocsIndexL3on = gmmHelper->getL3EnabledMOCS() >> 1;
    auto mocsIndexL1on = gmmHelper->getL1EnabledMOCS() >> 1;

    EXPECT_TRUE(mocsIndexL3on == csr.latestSentStatelessMocsConfig || mocsIndexL1on == csr.latestSentStatelessMocsConfig);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenNotAlignedPointerAndAlignedSizeWhenReadBufferIsCalledThenRecordedL3IndexIsL3Off) {

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    void *ptr = (void *)0x1039;

    cl_int retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                             CL_FALSE,
                                             0,
                                             MemoryConstants::cacheLineSize,
                                             ptr,
                                             nullptr,
                                             0,
                                             nullptr,
                                             nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto gmmHelper = pDevice->getGmmHelper();
    auto mocsIndexL3off = gmmHelper->getUncachedMOCS() >> 1;
    auto mocsIndexL3on = gmmHelper->getL3EnabledMOCS() >> 1;
    auto mocsIndexL1on = gmmHelper->getL1EnabledMOCS() >> 1;

    EXPECT_EQ(mocsIndexL3off, csr.latestSentStatelessMocsConfig);

    void *ptr2 = (void *)0x1040;

    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr2,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_TRUE(mocsIndexL3on == csr.latestSentStatelessMocsConfig || mocsIndexL1on == csr.latestSentStatelessMocsConfig);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenNotAlignedPointerAndSizeWhenBlockedReadBufferIsCalledThenRecordedL3IndexIsL3Off) {

    if (pCmdQ->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    auto ptr = reinterpret_cast<void *>(0x1039);

    auto userEvent = clCreateUserEvent(pCmdQ->getContextPtr(), nullptr);

    cl_int retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                             CL_FALSE,
                                             0,
                                             MemoryConstants::cacheLineSize,
                                             ptr,
                                             nullptr,
                                             1,
                                             &userEvent,
                                             nullptr);

    clSetUserEventStatus(userEvent, 0u);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto gmmHelper = pDevice->getGmmHelper();
    auto mocsIndexL3off = gmmHelper->getUncachedMOCS() >> 1;

    EXPECT_EQ(mocsIndexL3off, csr.latestSentStatelessMocsConfig);
    clReleaseEvent(userEvent);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenOOQWithEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueReadBuffer(srcBuffer.get(),
                                        CL_FALSE,
                                        0,
                                        MemoryConstants::cacheLineSize,
                                        ptr,
                                        nullptr,
                                        0,
                                        nullptr,
                                        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenOOQWithDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueReadBuffer(srcBuffer.get(),
                                        CL_FALSE,
                                        0,
                                        MemoryConstants::cacheLineSize,
                                        ptr,
                                        nullptr,
                                        0,
                                        nullptr,
                                        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenGpuHangAndBlockingCallAndOOQWithDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferWhenReadBufferIsExecutedThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    const auto enqueueResult = mockCommandQueueHw.enqueueReadBuffer(srcBuffer.get(),
                                                                    CL_TRUE,
                                                                    0,
                                                                    MemoryConstants::cacheLineSize,
                                                                    ptr,
                                                                    nullptr,
                                                                    0,
                                                                    nullptr,
                                                                    nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueReadBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueReadBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndNonZeroCopyBufferWhenReadBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueReadBuffer(nonZeroCopyBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}
HWTEST_F(EnqueueReadBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndNonZeroCopyWhenReadBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueReadBuffer(nonZeroCopyBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenCommandQueueWhenEnqueueReadBufferIsCalledThenItCallsNotifyFunction) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    auto retVal = mockCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                              CL_TRUE,
                                              0,
                                              MemoryConstants::cacheLineSize,
                                              ptr,
                                              nullptr,
                                              0,
                                              nullptr,
                                              nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockCmdQ->notifyEnqueueReadBufferCalled);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenCommandQueueWhenEnqueueReadBufferWithMapAllocationIsCalledThenItDoesntCallNotifyFunction) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    GraphicsAllocation mapAllocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    auto retVal = mockCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                              CL_TRUE,
                                              0,
                                              MemoryConstants::cacheLineSize,
                                              ptr,
                                              &mapAllocation,
                                              0,
                                              nullptr,
                                              nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(mockCmdQ->notifyEnqueueReadBufferCalled);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenEnqueueReadBufferCalledWhenLockedPtrInTransferPropertisIsAvailableThenItIsNotUnlocked) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueReadBuffer(buffer.get(),
                                         CL_TRUE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenForcedCpuCopyWhenEnqueueReadCompressedBufferThenDontCopyOnCpu) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    debugManager.flags.EnableCpuCacheForResources.set(true);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx(pClDevice);
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    static_cast<MemoryAllocation *>(graphicsAllocation)->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    retVal = mockCmdQ->enqueueReadBuffer(buffer.get(),
                                         CL_TRUE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(graphicsAllocation->isLocked());
    EXPECT_FALSE(mockCmdQ->cpuDataTransferHandlerCalled);

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), false);

    retVal = mockCmdQ->enqueueReadBuffer(buffer.get(),
                                         CL_TRUE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(graphicsAllocation->isLocked());
    EXPECT_TRUE(mockCmdQ->cpuDataTransferHandlerCalled);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenEnqueueReadBufferCalledWhenLockedPtrInTransferPropertisIsNotAvailableThenItIsNotUnlocked) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::system4KBPages);
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueReadBuffer(buffer.get(),
                                         CL_TRUE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenEnqueueReadBufferBlockingWhenAUBDumpAllocsOnEnqueueReadOnlyIsOnThenBufferShouldBeSetDumpable) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);

    ASSERT_FALSE(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->isAllocDumpable());
    cl_int retVal = CL_SUCCESS;
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                      CL_TRUE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->isAllocDumpable());
    EXPECT_TRUE(srcBuffer->forceDisallowCPUCopy);
}

HWTEST_F(EnqueueReadBufferTypeTest, givenEnqueueReadBufferNonBlockingWhenAUBDumpAllocsOnEnqueueReadOnlyIsOnThenBufferShouldntBeSetDumpable) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);

    ASSERT_FALSE(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->isAllocDumpable());
    cl_int retVal = CL_SUCCESS;
    void *ptr = nonZeroCopyBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->isAllocDumpable());
    EXPECT_FALSE(srcBuffer->forceDisallowCPUCopy);
}

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueReadBufferWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    retVal = pCmdQ->enqueueReadBuffer(buffer.get(),
                                      CL_FALSE,
                                      0,
                                      MemoryConstants::cacheLineSize,
                                      ptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

struct EnqueueReadBufferHw : public ::testing::Test {

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    MockBuffer srcBuffer;
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
};

using EnqueueReadBufferStatelessTest = EnqueueReadBufferHw;

HWTEST_F(EnqueueReadBufferStatelessTest, WhenReadingBufferStatelessThenSuccessIsReturned) {

    auto pCmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = pCmdQ->enqueueReadBuffer(&srcBuffer,
                                           CL_FALSE,
                                           0,
                                           MemoryConstants::cacheLineSize,
                                           missAlignedPtr,
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

using EnqueueReadBufferStatefulTest = EnqueueReadBufferHw;

HWTEST_F(EnqueueReadBufferStatefulTest, WhenReadingBufferStatefulThenSuccessIsReturned) {

    auto pCmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    if (pCmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = pCmdQ->enqueueReadBuffer(&srcBuffer,
                                           CL_FALSE,
                                           0,
                                           MemoryConstants::cacheLineSize,
                                           missAlignedPtr,
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueReadBufferHw, givenHostPtrIsFromMappedBufferWhenReadBufferIsCalledThenReuseGraphicsAllocation) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DisableZeroCopyForBuffers.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);

    MockCommandQueueHw<FamilyType> queue(context.get(), device.get(), nullptr);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();

    BufferDefaults::context = context.get();
    auto bufferForMap = clUniquePtr(BufferHelper<>::create());
    auto bufferForRead = clUniquePtr(BufferHelper<>::create());

    cl_int retVal{};
    void *mappedPtr = queue.enqueueMapBuffer(bufferForMap.get(), CL_TRUE, CL_MAP_READ, 0, bufferForMap->getSize(), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);

    MapOperationsHandler *mapOperationsHandler = context->getMapOperationsStorage().getHandlerIfExists(bufferForMap.get());
    EXPECT_NE(nullptr, mapOperationsHandler);
    MapInfo mapInfo{};
    EXPECT_TRUE(mapOperationsHandler->find(mappedPtr, mapInfo));
    EXPECT_NE(nullptr, mapInfo.graphicsAllocation);

    auto unmappedPtr = std::make_unique<char[]>(bufferForRead->getSize());
    retVal = queue.enqueueReadBuffer(bufferForRead.get(), CL_TRUE, 0, bufferForRead->getSize(), unmappedPtr.get(), nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);

    retVal = queue.enqueueReadBuffer(bufferForRead.get(), CL_TRUE, 0, bufferForRead->getSize(), mappedPtr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
}

struct ReadBufferStagingBufferTest : public EnqueueReadBufferHw {
    void SetUp() override {
        REQUIRE_SVM_OR_SKIP(defaultHwInfo);
        EnqueueReadBufferHw::SetUp();
    }

    void TearDown() override {
        EnqueueReadBufferHw::TearDown();
    }
    constexpr static size_t chunkSize = MemoryConstants::pageSize;

    unsigned char ptr[MemoryConstants::cacheLineSize];
    MockBuffer buffer;
    cl_queue_properties props = {};
};

HWTEST_F(ReadBufferStagingBufferTest, whenEnqueueStagingReadBufferCalledThenReturnSuccess) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, &buffer, false, 0, buffer.getSize(), ptr, nullptr);
    EXPECT_TRUE(mockCommandQueueHw.flushCalled);
    EXPECT_EQ(res, CL_SUCCESS);
    EXPECT_EQ(1ul, mockCommandQueueHw.enqueueReadBufferCounter);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
}

HWTEST_F(ReadBufferStagingBufferTest, whenHostPtrRegisteredThenDontUseStagingUntilEventCompleted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);

    cl_event event;
    auto retVal = mockCommandQueueHw.enqueueReadBuffer(&buffer,
                                                       CL_FALSE,
                                                       0,
                                                       MemoryConstants::cacheLineSize,
                                                       ptr,
                                                       nullptr,
                                                       0,
                                                       nullptr,
                                                       &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pEvent = castToObjectOrAbort<Event>(event);

    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));
    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));

    pEvent->updateExecutionStatus();
    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));

    pEvent->release();
}

HWTEST_F(ReadBufferStagingBufferTest, whenHostPtrRegisteredThenDontUseStagingUntilFinishCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);

    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));
    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));

    mockCommandQueueHw.finish();
    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_READ_BUFFER, false, false));
}

HWTEST_F(ReadBufferStagingBufferTest, whenEnqueueStagingReadBufferCalledWithLargeSizeThenSplitTransfer) {
    auto hostPtr = new unsigned char[chunkSize * 4];
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(),
                                                                            0,
                                                                            chunkSize * 4,
                                                                            nullptr,
                                                                            retVal));
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, buffer.get(), false, 0, chunkSize * 4, hostPtr, nullptr);
    EXPECT_TRUE(mockCommandQueueHw.flushCalled);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(res, CL_SUCCESS);
    EXPECT_EQ(4ul, mockCommandQueueHw.enqueueReadBufferCounter);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);

    delete[] hostPtr;
}

HWTEST_F(ReadBufferStagingBufferTest, whenEnqueueStagingReadBufferCalledWithEventThenReturnValidEvent) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_READ_BUFFER;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    cl_event event;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_EQ(expectedLastCmd, mockCommandQueueHw.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(ReadBufferStagingBufferTest, givenOutOfOrderQueueWhenEnqueueStagingReadBufferCalledWithSingleTransferThenNoBarrierEnqueued) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_READ_BUFFER;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.setOoqEnabled();
    cl_event event;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_EQ(expectedLastCmd, mockCommandQueueHw.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(ReadBufferStagingBufferTest, givenCmdQueueWithProfilingWhenEnqueueStagingReadBufferThenTimestampsSetCorrectly) {
    cl_event event;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.setProfilingEnabled();
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_FALSE(pEvent->isCPUProfilingPath());
    EXPECT_TRUE(pEvent->isProfilingEnabled());

    clReleaseEvent(event);
}

HWTEST_F(ReadBufferStagingBufferTest, whenEnqueueStagingReadBufferFailedThenPropagateErrorCode) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.enqueueReadBufferCallBase = false;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_READ_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, nullptr);

    EXPECT_EQ(res, CL_INVALID_OPERATION);
    EXPECT_EQ(1ul, mockCommandQueueHw.enqueueReadBufferCounter);
}

HWTEST_F(ReadBufferStagingBufferTest, whenIsValidForStagingTransferCalledThenReturnCorrectValue) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto isStagingBuffersEnabled = device->getProductHelper().isStagingBuffersEnabled();
    unsigned char ptr[16];

    EXPECT_EQ(isStagingBuffersEnabled, mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, 16, CL_COMMAND_READ_BUFFER, false, false));
}

HWTEST_F(ReadBufferStagingBufferTest, whenIsValidForStagingTransferCalledAndCpuCopyAllowedThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    unsigned char ptr[16];

    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, 16, CL_COMMAND_READ_BUFFER, true, false));
}