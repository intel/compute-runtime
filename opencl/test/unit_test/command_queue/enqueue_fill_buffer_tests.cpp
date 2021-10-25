/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/command_queue/enqueue_fill_buffer_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_commands_common_validation.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "test.h"

#include "reg_configs_common.h"

using namespace NEO;

typedef Test<EnqueueFillBufferFixture> EnqueueFillBufferCmdTests;

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenTaskCountIsAlignedWithCsr) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillBufferCmdTests, WhenFillingBufferThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    enqueueFillBuffer<FamilyType>();

    auto *cmd = (GPGPU_WALKER *)cmdWalker;
    ASSERT_NE(nullptr, cmd);

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

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenIndirectDataGetsAdded) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    auto kernelDescriptor = &kernel->getKernelInfo().kernelDescriptor;

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernelDescriptor, rootDeviceIndex));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->usesBindfulAddressingForBuffers()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, GivenRightLeftoverWhenFillingBufferThenFillBufferRightLeftoverKernelUsed) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {0, 0, 0};
    dc.size = {EnqueueFillBufferTraits::patternSize, 0, 0};

    MultiDispatchInfo mdi(dc);
    builder.buildDispatchInfos(mdi);
    EXPECT_EQ(1u, mdi.size());

    auto kernel = mdi.begin()->getKernel();
    EXPECT_STREQ("FillBufferRightLeftover", kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str());

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, GivenMiddleWhenFillingBufferThenFillBufferMiddleKernelUsed) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {0, 0, 0};
    dc.size = {MemoryConstants::cacheLineSize, 0, 0};

    MultiDispatchInfo mdi(dc);
    builder.buildDispatchInfos(mdi);
    EXPECT_EQ(1u, mdi.size());

    auto kernel = mdi.begin()->getKernel();
    EXPECT_STREQ("FillBufferMiddle", kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str());

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, GivenLeftLeftoverWhenFillingBufferThenFillBufferLeftLeftoverKernelUsed) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {EnqueueFillBufferTraits::patternSize, 0, 0};
    dc.size = {EnqueueFillBufferTraits::patternSize, 0, 0};

    MultiDispatchInfo mdi(dc);
    builder.buildDispatchInfos(mdi);
    EXPECT_EQ(1u, mdi.size());

    auto kernel = mdi.begin()->getKernel();
    EXPECT_STREQ("FillBufferLeftLeftover", kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str());

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenL3ProgrammingIsCorrect) {
    enqueueFillBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillBufferCmdTests, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueFillBuffer<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, pIOH->getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                                         ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillBufferCmdTests, WhenFillingBufferThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueFillBuffer<FamilyType>();

    auto *cmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)cmdMediaInterfaceDescriptorLoad;

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
    FamilyType::PARSE::template validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorMediaInterfaceDescriptorLoad);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillBufferCmdTests, WhenFillingBufferThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    enqueueFillBuffer<FamilyType>();

    // Extract the IDD
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    auto &IDD = *(INTERFACE_DESCRIPTOR_DATA *)(cmdInterfaceDescriptorData);

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)IDD.getKernelStartPointerHigh() << 32) + IDD.getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, IDD.getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, IDD.getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, IDD.getConstantIndirectUrbEntryReadLength());
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenNumberOfPipelineSelectsIsOne) {
    enqueueFillBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueFillBufferCmdTests, WhenFillingBufferThenMediaVfeStateIsSetCorrectly) {
    enqueueFillBuffer<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenArgumentZeroShouldMatchDestAddress) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    enqueueFillBuffer<FamilyType>();

    // Extract the kernel used

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(kernel->getKernelInfo(), 0u, pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0), rootDeviceIndex);

    EXPECT_EQ((void *)((uintptr_t)buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress()), *pArgument);

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenArgumentTwoShouldMatchPatternPtr) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    enqueueFillBuffer<FamilyType>();

    // Extract the kernel used
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBuffer,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(kernel->getKernelInfo(), 2u, pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0), rootDeviceIndex);
    EXPECT_NE(nullptr, *pArgument);

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferStatelessThenStatelessKernelIsUsed) {
    auto patternAllocation = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), EnqueueFillBufferTraits::patternSize});

    // Extract the kernel used
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::FillBufferStateless,
                                                                            pCmdQ->getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    MemObj patternMemObj(&this->context, 0, {}, 0, 0, alignUp(EnqueueFillBufferTraits::patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), GraphicsAllocationHelper::toMultiGraphicsAllocation(patternAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {EnqueueFillBufferTraits::offset, 0, 0};
    dc.size = {EnqueueFillBufferTraits::size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
    EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());

    context.getMemoryManager()->freeGraphicsMemory(patternAllocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenPatternShouldBeCopied) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
    GraphicsAllocation *allocation = csr.getAllocationsForReuse().peekHead();

    while (allocation != nullptr) {
        if ((allocation->getUnderlyingBufferSize() >= sizeof(float)) &&
            (allocation->getUnderlyingBuffer() != nullptr) &&
            (*(static_cast<float *>(allocation->getUnderlyingBuffer())) == EnqueueFillBufferHelper<>::Traits::pattern[0]) &&
            (pCmdQ->taskCount == allocation->getTaskCount(csr.getOsContext().getContextId()))) {
            break;
        }
        allocation = allocation->next;
    }

    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(&EnqueueFillBufferHelper<>::Traits::pattern[0], allocation->getUnderlyingBuffer());
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenPatternShouldBeAligned) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    GraphicsAllocation *allocation = csr.getAllocationsForReuse().peekHead();

    while (allocation != nullptr) {
        if ((allocation->getUnderlyingBufferSize() >= sizeof(float)) &&
            (allocation->getUnderlyingBuffer() != nullptr) &&
            (*(static_cast<float *>(allocation->getUnderlyingBuffer())) == EnqueueFillBufferHelper<>::Traits::pattern[0]) &&
            (pCmdQ->taskCount == allocation->getTaskCount(csr.getOsContext().getContextId()))) {
            break;
        }
        allocation = allocation->next;
    }

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(alignUp(allocation->getUnderlyingBuffer(), MemoryConstants::cacheLineSize), allocation->getUnderlyingBuffer());
    EXPECT_EQ(alignUp(allocation->getUnderlyingBufferSize(), MemoryConstants::cacheLineSize), allocation->getUnderlyingBufferSize());
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillBufferIsCalledTwiceThenPatternAllocationIsReused) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getAllocationsForReuse().peekIsEmpty());
    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
    GraphicsAllocation *allocation = csr.getAllocationsForReuse().peekHead();
    EnqueueFillBufferHelper<>::enqueueFillBuffer(pCmdQ, buffer);
    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
    EXPECT_NE(csr.getAllocationsForReuse().peekHead(), nullptr);
    EXPECT_EQ(allocation, csr.getAllocationsForReuse().peekHead());
    EXPECT_EQ(csr.getAllocationsForReuse().peekTail(), allocation);
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenPatternOfSizeOneByteShouldGetPreparedForMiddleKernel) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getAllocationsForReuse().peekIsEmpty());
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    const uint8_t pattern[1] = {0x55};
    const size_t patternSize = sizeof(pattern);
    const size_t offset = 0;
    const size_t size = 4 * patternSize;
    const uint8_t output[4] = {0x55, 0x55, 0x55, 0x55};

    auto retVal = clEnqueueFillBuffer(
        pCmdQ,
        dstBuffer.get(),
        pattern,
        patternSize,
        offset,
        size,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    GraphicsAllocation *allocation = csr.getAllocationsForReuse().peekHead();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), output, size));
}

HWTEST_F(EnqueueFillBufferCmdTests, WhenFillingBufferThenPatternOfSizeTwoBytesShouldGetPreparedForMiddleKernel) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getAllocationsForReuse().peekIsEmpty());
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    const uint8_t pattern[2] = {0x55, 0xAA};
    const size_t patternSize = sizeof(pattern);
    const size_t offset = 0;
    const size_t size = 2 * patternSize;
    const uint8_t output[4] = {0x55, 0xAA, 0x55, 0xAA};

    auto retVal = clEnqueueFillBuffer(
        pCmdQ,
        dstBuffer.get(),
        pattern,
        patternSize,
        offset,
        size,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    GraphicsAllocation *allocation = csr.getAllocationsForReuse().peekHead();
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), output, size));
}

HWTEST_F(EnqueueFillBufferCmdTests, givenEnqueueFillBufferWhenPatternAllocationIsObtainedThenItsTypeShouldBeSetToFillPattern) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    ASSERT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    const uint8_t pattern[1] = {0x55};
    const size_t patternSize = sizeof(pattern);
    const size_t offset = 0;
    const size_t size = patternSize;

    auto retVal = clEnqueueFillBuffer(
        pCmdQ,
        dstBuffer.get(),
        pattern,
        patternSize,
        offset,
        size,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());

    GraphicsAllocation *patternAllocation = csr.getAllocationsForReuse().peekHead();
    ASSERT_NE(nullptr, patternAllocation);

    EXPECT_EQ(GraphicsAllocation::AllocationType::FILL_PATTERN, patternAllocation->getAllocationType());
}

struct EnqueueFillBufferHw : public ::testing::Test {

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockClDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    const uint8_t pattern[1] = {0x55};
    const size_t patternSize = sizeof(pattern);
    const size_t offset = 0;
    const size_t size = patternSize;
    MockBuffer dstBuffer;
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
};

using EnqueueFillBufferStatelessTest = EnqueueFillBufferHw;

HWTEST_F(EnqueueFillBufferStatelessTest, givenBuffersWhenFillingBufferStatelessThenSuccessIsReturned) {
    auto pCmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    dstBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = pCmdQ->enqueueFillBuffer(
        &dstBuffer,
        pattern,
        patternSize,
        offset,
        size,
        0,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);
}

using EnqueueFillBufferStatefullTest = EnqueueFillBufferHw;

HWTEST_F(EnqueueFillBufferStatefullTest, givenBuffersWhenFillingBufferStatefullThenSuccessIsReturned) {
    auto pCmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    dstBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = pCmdQ->enqueueFillBuffer(
        &dstBuffer,
        pattern,
        patternSize,
        offset,
        size,
        0,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);
}
