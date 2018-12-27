/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/buffer_operations_fixture.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "reg_configs_common.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/memory_manager/allocations_list.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "test.h"

using namespace OCLRT;

HWTEST_F(EnqueueWriteBufferTypeTest, null_mem_object) {
    auto data = 1;
    auto retVal = clEnqueueWriteBuffer(
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

HWTEST_F(EnqueueWriteBufferTypeTest, null_user_pointer) {
    auto data = 1;

    auto retVal = clEnqueueWriteBuffer(
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

HWTEST_F(EnqueueWriteBufferTypeTest, alignsToCSR_Blocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueWriteBufferTypeTest, alignsToCSR_NonBlocking) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteBufferTypeTest, GPGPUWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();

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
        cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD32 ? 32 : cmd->getSimdSize() == GPGPU_WALKER::SIMD_SIZE_SIMD16 ? 16 : 8;
    uint64_t simdMask = (1ull << simd) - 1;

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueWriteBufferTypeTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueWriteBufferTypeTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueWriteBufferTypeTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pCmdQ->getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                               pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = srcBuffer.get();
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernel));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->requiresSshForBuffers()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueWriteBufferTypeTest, LoadRegisterImmediateL3CNTLREG) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteBufferTypeTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();

    validateStateBaseAddress<FamilyType>(this->pCmdQ->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteBufferTypeTest, MediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();

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

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteBufferTypeTest, InterfaceDescriptorData) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();

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

    // Extract the IDD
    auto &IDD = *(INTERFACE_DESCRIPTOR_DATA *)(DSH + iddStart);

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)IDD.getKernelStartPointerHigh() << 32) + IDD.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, IDD.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, IDD.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, IDD.getConstantIndirectUrbEntryReadLength());
}

HWTEST_F(EnqueueWriteBufferTypeTest, PipelineSelect) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueWriteBufferTypeTest, MediaVFEState) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenOOQWithEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferTrueWhenWriteBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
                                         CL_FALSE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
    pCmdOOQ->flush();
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenOOQWithDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrZeroCopyBufferWhenWriteBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
                                         CL_FALSE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdOOQ->taskLevel, 0u);
    pCmdOOQ->flush();
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
                                       CL_FALSE,
                                       0,
                                       MemoryConstants::cacheLineSize,
                                       ptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    cl_int retVal = CL_SUCCESS;
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
                                       CL_FALSE,
                                       0,
                                       MemoryConstants::cacheLineSize,
                                       ptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 0u);
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       CL_FALSE,
                                       0,
                                       MemoryConstants::cacheLineSize,
                                       ptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrNonZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       CL_FALSE,
                                       0,
                                       MemoryConstants::cacheLineSize,
                                       ptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pCmdQ->taskLevel, 1u);
}

HWTEST_F(EnqueueWriteBufferTypeTest, givenEnqueueWriteBufferCalledWhenLockedPtrInTransferPropertisIsAvailableThenItIsUnlocked) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceResourceLockOnTransferCalls.set(true);
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);

    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.setMemoryManager(&memoryManager);
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation())->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
                                          0,
                                          MemoryConstants::cacheLineSize,
                                          ptr,
                                          0,
                                          nullptr,
                                          nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, memoryManager.unlockResourceCalled);
}

HWTEST_F(EnqueueWriteBufferTypeTest, givenEnqueueWriteBufferCalledWhenLockedPtrInTransferPropertisIsNotAvailableThenItIsNotUnlocked) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceResourceLockOnTransferCalls.set(true);
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);

    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.setMemoryManager(&memoryManager);
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation())->overrideMemoryPool(MemoryPool::System4KBPages);
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
                                          0,
                                          MemoryConstants::cacheLineSize,
                                          ptr,
                                          0,
                                          nullptr,
                                          nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);
}

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueWriteBufferWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    retVal = pCmdQ->enqueueWriteBuffer(buffer.get(),
                                       CL_FALSE,
                                       0,
                                       MemoryConstants::cacheLineSize,
                                       ptr,
                                       0,
                                       nullptr,
                                       nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

HWTEST_F(EnqueueWriteBufferTypeTest, givenNotAlignedPointerAndAlignedSizeWhenWriteBufferIsCalledThenHostGraphicsAllocationHasCorrectOffset) {
    void *ptr = (void *)0x1039;

    cl_int retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                              CL_FALSE,
                                              0,
                                              MemoryConstants::cacheLineSize,
                                              ptr,
                                              0,
                                              nullptr,
                                              nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto allocation = csr.getTemporaryAllocations().peekHead();
    while (allocation && allocation->getUnderlyingBuffer() != alignDown(ptr, 4)) {
        allocation = allocation->next;
    }

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ((void *)allocation->getGpuAddressToPatch(), ptr);
}
