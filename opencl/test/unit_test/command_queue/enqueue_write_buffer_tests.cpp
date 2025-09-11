/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/buffer_operations_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_builder.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

HWTEST_F(EnqueueWriteBufferTypeTest, GivenNullBufferWhenWrtingBufferThenInvalidMemObjectErrorIsReturned) {
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

HWTEST_F(EnqueueWriteBufferTypeTest, GivenNullUserPointerWhenWritingBufferThenInvalidValueErrorIsReturned) {
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

HWTEST_F(EnqueueWriteBufferTypeTest, GivenBlockingEnqueueWhenWritingBufferThenTaskLevelIsNotIncremented) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;
    auto oldCsrTaskLevel = csr.peekTaskLevel();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), CL_TRUE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(oldCsrTaskLevel, pCmdQ->taskLevel);
}

HWTEST_F(EnqueueWriteBufferTypeTest, GivenGpuHangAndBlockingEnqueueWhenWritingBufferThenOutOfResourcesIsReturned) {
    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    srcBuffer->forceDisallowCPUCopy = true;
    const auto enqueueResult = EnqueueWriteBufferHelper<>::enqueueWriteBuffer(&mockCommandQueueHw, srcBuffer.get(), CL_TRUE);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueWriteBufferTypeTest, GivenNonBlockingEnqueueWhenWritingBufferThenTaskLevelIsIncremented) {
    // this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), CL_FALSE);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);

    auto cmdQTaskLevel = pCmdQ->taskLevel;
    if (!csr.isUpdateTagFromWaitEnabled()) {
        cmdQTaskLevel++;
    }
    EXPECT_EQ(csr.peekTaskLevel(), cmdQTaskLevel);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueWriteBufferTypeTest, WhenWritingBufferThenCommandsAreProgrammedCorrectly) {
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

HWTEST_F(EnqueueWriteBufferTypeTest, WhenWritingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueWriteBufferTypeTest, WhenWritingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueWriteBufferTypeTest, WhenWritingBufferThenIndirectDataIsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    srcBuffer->forceDisallowCPUCopy = true;
    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, srcBuffer.get(), EnqueueWriteBufferTraits::blocking);

    auto builtInType = EBuiltInOps::copyBufferToBuffer;

    if (static_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ)->isForceStateless) {
        builtInType = EBuiltInOps::copyBufferToBufferStateless;
    }

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);

    builtInType = adjustBuiltInType(heaplessEnabled, builtInType);
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            pCmdQ->getClDevice());

    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = EnqueueWriteBufferTraits::hostPtr;
    dc.dstMemObj = srcBuffer.get();
    dc.dstOffset = {EnqueueWriteBufferTraits::offset, 0, 0};
    dc.size = {srcBuffer->getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    auto kernelDescriptor = &kernel->getKernelInfo().kernelDescriptor;

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernelDescriptor, rootDeviceIndex));

    auto crossThreadDatSize = kernel->getCrossThreadDataSize();
    auto inlineDataSize = UnitTestHelper<FamilyType>::getInlineDataSize(heaplessEnabled);
    bool crossThreadDataFitsInInlineData = (crossThreadDatSize <= inlineDataSize);
    bool isUsingImplicitArgs = kernel->getImplicitArgs() != nullptr;

    if (crossThreadDataFitsInInlineData && !isUsingImplicitArgs) {
        EXPECT_EQ(iohBefore, pIOH->getUsed());
    } else {
        EXPECT_NE(iohBefore, pIOH->getUsed());
    }

    if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueWriteBufferTypeTest, WhenWritingBufferThenL3ProgrammingIsCorrect) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueWriteBufferTypeTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !gfxCoreHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueWriteBufferTypeTest, WhenWritingBufferThenMediaInterfaceDescriptorIsCorrect) {
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
    FamilyType::Parse::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorCmd);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueWriteBufferTypeTest, WhenWritingBufferThenInterfaceDescriptorDataIsCorrect) {
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

HWTEST2_F(EnqueueWriteBufferTypeTest, WhenWritingBufferThenOnePipelineSelectIsProgrammed, IsAtMostXeCore) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, EnqueueWriteBufferTypeTest, WhenWritingBufferThenMediaVfeStateIsCorrect) {
    srcBuffer->forceDisallowCPUCopy = true;
    enqueueWriteBuffer<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenOOQWithEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrAndZeroCopyBufferTrueWhenWriteBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
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
    pCmdOOQ->flush();
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenOOQWithDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrZeroCopyBufferWhenWriteBufferIsExecutedThenTaskLevelNotIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    UltCommandStreamReceiver<FamilyType> &mockCsr =
        reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(pCmdOOQ->getGpgpuCommandStreamReceiver());
    mockCsr.useNewResourceImplicitFlush = false;
    mockCsr.useGpuIdleImplicitFlush = false;

    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdOOQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
                                         CL_FALSE,
                                         0,
                                         MemoryConstants::cacheLineSize,
                                         ptr,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, pCmdOOQ->taskLevel);
    pCmdOOQ->flush();
}
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
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
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    void *ptr = zeroCopyBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(zeroCopyBuffer.get(),
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
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
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
HWTEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrNonZeroCopyBufferEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
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

HWTEST_F(EnqueueWriteBufferTypeTest, givenEnqueueWriteBufferCalledWhenLockedPtrInTransferPropertisIsAvailableThenItIsNotUnlocked) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
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

HWTEST_F(EnqueueWriteBufferTypeTest, givenForcedCpuCopyWhenEnqueueWriteCompressedBufferThenDontCopyOnCpu) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);
    debugManager.flags.EnableCpuCacheForResources.set(true);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    auto allocation = static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()));

    allocation->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    MockBuffer::setAllocationType(allocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
                                          0,
                                          MemoryConstants::cacheLineSize,
                                          ptr,
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(allocation->isLocked());
    EXPECT_FALSE(mockCmdQ->cpuDataTransferHandlerCalled);

    MockBuffer::setAllocationType(allocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), false);

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
                                          0,
                                          MemoryConstants::cacheLineSize,
                                          ptr,
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_TRUE(mockCmdQ->cpuDataTransferHandlerCalled);
}

HWTEST_F(EnqueueWriteBufferTypeTest, givenEnqueueWriteBufferCalledWhenLockedPtrInTransferPropertisIsNotAvailableThenItIsNotUnlocked) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockContext ctx;
    cl_int retVal;
    ctx.memoryManager = &memoryManager;
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> buffer(Buffer::create(&ctx, 0, 1, nullptr, retVal));
    static_cast<MemoryAllocation *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()))->overrideMemoryPool(MemoryPool::system4KBPages);
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();

    retVal = mockCmdQ->enqueueWriteBuffer(buffer.get(),
                                          CL_FALSE,
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

using NegativeFailAllocationTest = Test<NegativeFailAllocationCommandEnqueueBaseFixture>;

HWTEST_F(NegativeFailAllocationTest, givenEnqueueWriteBufferWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    cl_int retVal = CL_SUCCESS;
    retVal = pCmdQ->enqueueWriteBuffer(buffer.get(),
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

struct EnqueueWriteBufferHw : public ::testing::Test {

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

using EnqueueReadWriteStatelessTest = EnqueueWriteBufferHw;

HWTEST_F(EnqueueReadWriteStatelessTest, WhenWritingBufferStatelessThenSuccessIsReturned) {

    auto pCmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = pCmdQ->enqueueWriteBuffer(&srcBuffer,
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

using EnqueueWriteBufferStatefulTest = EnqueueWriteBufferHw;

HWTEST2_F(EnqueueWriteBufferStatefulTest, WhenWritingBufferStatefulThenSuccessIsReturned, IsStatefulBufferPreferredForProduct) {

    auto pCmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    if (pCmdQ->getHeaplessModeEnabled()) {
        GTEST_SKIP();
    }
    void *missAlignedPtr = reinterpret_cast<void *>(0x1041);
    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = pCmdQ->enqueueWriteBuffer(&srcBuffer,
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

HWTEST_F(EnqueueWriteBufferHw, givenHostPtrIsFromMappedBufferWhenWriteBufferIsCalledThenReuseGraphicsAllocation) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DisableZeroCopyForBuffers.set(1);
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(0);

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
    retVal = queue.enqueueWriteBuffer(bufferForRead.get(), CL_TRUE, 0, bufferForRead->getSize(), unmappedPtr.get(), nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);

    retVal = queue.enqueueWriteBuffer(bufferForRead.get(), CL_TRUE, 0, bufferForRead->getSize(), mappedPtr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
}

struct WriteBufferStagingBufferTest : public EnqueueWriteBufferHw {
    void SetUp() override {
        EnqueueWriteBufferHw::SetUp();
    }

    void TearDown() override {
        EnqueueWriteBufferHw::TearDown();
    }
    constexpr static size_t chunkSize = MemoryConstants::pageSize;

    unsigned char ptr[MemoryConstants::cacheLineSize];
    MockBuffer buffer;
    cl_queue_properties props = {};
};

HWTEST_F(WriteBufferStagingBufferTest, whenEnqueueStagingWriteBufferCalledThenReturnSuccess) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, &buffer, false, 0, buffer.getSize(), ptr, nullptr);
    EXPECT_TRUE(mockCommandQueueHw.flushCalled);
    EXPECT_EQ(res, CL_SUCCESS);
    EXPECT_EQ(1ul, mockCommandQueueHw.enqueueWriteBufferCounter);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
}

HWTEST_F(WriteBufferStagingBufferTest, whenHostPtrRegisteredThenDontUseStagingUntilEventCompleted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);

    cl_event event;
    auto retVal = mockCommandQueueHw.enqueueWriteBuffer(&buffer,
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

    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));
    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));

    pEvent->updateExecutionStatus();
    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));

    pEvent->release();
}

HWTEST_F(WriteBufferStagingBufferTest, whenHostPtrRegisteredThenDontUseStagingUntilFinishCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);

    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));
    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));

    mockCommandQueueHw.finish(false);
    EXPECT_TRUE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, MemoryConstants::cacheLineSize, CL_COMMAND_WRITE_BUFFER, false, false));
}

HWTEST_F(WriteBufferStagingBufferTest, whenEnqueueStagingWriteBufferCalledWithLargeSizeThenSplitTransfer) {
    auto hostPtr = new unsigned char[chunkSize * 4];
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(),
                                                                            0,
                                                                            chunkSize * 4,
                                                                            nullptr,
                                                                            retVal));
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, buffer.get(), false, 0, chunkSize * 4, hostPtr, nullptr);
    EXPECT_TRUE(mockCommandQueueHw.flushCalled);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(res, CL_SUCCESS);
    EXPECT_EQ(4ul, mockCommandQueueHw.enqueueWriteBufferCounter);
    auto &csr = device->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);

    delete[] hostPtr;
}

HWTEST_F(WriteBufferStagingBufferTest, whenEnqueueStagingWriteBufferCalledWithEventThenReturnValidEvent) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_WRITE_BUFFER;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    cl_event event;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_EQ(expectedLastCmd, mockCommandQueueHw.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(WriteBufferStagingBufferTest, givenOutOfOrderQueueWhenEnqueueStagingWriteBufferCalledWithSingleTransferThenNoBarrierEnqueued) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_WRITE_BUFFER;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.setOoqEnabled();
    cl_event event;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_EQ(expectedLastCmd, mockCommandQueueHw.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(WriteBufferStagingBufferTest, givenCmdQueueWithProfilingWhenEnqueueStagingWriteBufferThenTimestampsSetCorrectly) {
    cl_event event;
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.setProfilingEnabled();
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, &event);
    EXPECT_EQ(res, CL_SUCCESS);

    auto pEvent = (Event *)event;
    EXPECT_FALSE(pEvent->isCPUProfilingPath());
    EXPECT_TRUE(pEvent->isProfilingEnabled());

    clReleaseEvent(event);
}

HWTEST_F(WriteBufferStagingBufferTest, whenEnqueueStagingWriteBufferFailedThenPropagateErrorCode) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.enqueueWriteBufferCallBase = false;
    auto res = mockCommandQueueHw.enqueueStagingBufferTransfer(CL_COMMAND_WRITE_BUFFER, &buffer, false, 0, MemoryConstants::cacheLineSize, ptr, nullptr);

    EXPECT_EQ(res, CL_INVALID_OPERATION);
    EXPECT_EQ(1ul, mockCommandQueueHw.enqueueWriteBufferCounter);
}

HWTEST_F(WriteBufferStagingBufferTest, whenIsValidForStagingTransferCalledThenReturnCorrectValue) {
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    auto isStagingBuffersEnabled = device->getProductHelper().isStagingBuffersEnabled();
    unsigned char ptr[16];

    EXPECT_EQ(isStagingBuffersEnabled, mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, 16, CL_COMMAND_WRITE_BUFFER, false, false));
}

HWTEST_F(WriteBufferStagingBufferTest, whenIsValidForStagingTransferCalledAndCpuCopyAllowedThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnWriteBuffer.set(1);
    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    unsigned char ptr[16];

    EXPECT_FALSE(mockCommandQueueHw.isValidForStagingTransfer(&buffer, ptr, 16, CL_COMMAND_WRITE_BUFFER, true, false));
}

HWTEST_F(EnqueueWriteBufferTypeTest, given4gbBufferAndIsForceStatelessIsFalseWhenEnqueueWriteBufferCallThenStatelessIsUsed) {
    struct FourGbMockBuffer : MockBuffer {
        size_t getSize() const override { return static_cast<size_t>(4ull * MemoryConstants::gigaByte); }
    };

    if (is32bit) {
        GTEST_SKIP();
    }

    auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ);
    mockCmdQ->isForceStateless = false;

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(true, pCmdQ->getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, pCmdQ->getClDevice())));

    FourGbMockBuffer buffer;

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    EnqueueWriteBufferHelper<>::enqueueWriteBuffer(pCmdQ, &buffer);

    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_EQ(mockBuilder, newBuilder.get());
}