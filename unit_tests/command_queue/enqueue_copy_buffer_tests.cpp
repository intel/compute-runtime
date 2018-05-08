/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "reg_configs_common.h"
#include "runtime/helpers/dispatch_info.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/command_queue/enqueue_copy_buffer_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "test.h"
#include <memory>

using namespace OCLRT;

HWTEST_F(EnqueueCopyBufferTest, null_src_mem_object) {
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto retVal = clEnqueueCopyBuffer(
        pCmdQ,
        nullptr,
        dstBuffer.get(),
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueCopyBufferTest, null_dst_mem_object) {
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto retVal = clEnqueueCopyBuffer(
        pCmdQ,
        srcBuffer.get(),
        nullptr,
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

HWTEST_F(EnqueueCopyBufferTest, invalid_value) {
    auto retVal = clEnqueueCopyBuffer(
        pCmdQ,
        srcBuffer,
        dstBuffer,
        0,
        8,
        128,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(EnqueueCopyBufferTest, alignsToCSR) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueCopyBuffer<FamilyType>();
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWTEST_F(EnqueueCopyBufferTest, GPGPUWalker) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBuffer<FamilyType>();

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

HWTEST_F(EnqueueCopyBufferTest, bumpsTaskLevel) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyBuffer<FamilyType>();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferTest, addsCommands) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyBuffer<FamilyType>();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueCopyBufferTest, addsIndirectData) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueCopyBuffer<FamilyType>();

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                          pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.srcMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();

    EXPECT_NE(dshBefore, pDSH->getUsed());
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->requiresSshForBuffers()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueCopyBufferTest, LoadRegisterImmediateL3CNTLREG) {
    enqueueCopyBuffer<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWTEST_F(EnqueueCopyBufferTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBuffer<FamilyType>();
    validateStateBaseAddress<FamilyType>(this->pDevice->getCommandStreamReceiver().getMemoryManager()->getInternalHeapBaseAddress(),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWTEST_F(EnqueueCopyBufferTest, MediaInterfaceDescriptorLoad) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBuffer<FamilyType>();

    auto *cmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)cmdMediaInterfaceDescriptorLoad;
    ASSERT_NE(nullptr, cmd);

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

HWTEST_F(EnqueueCopyBufferTest, InterfaceDescriptorData) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBuffer<FamilyType>();

    auto cmdIDD = (INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)cmdIDD->getKernelStartPointerHigh() << 32) + cmdIDD->getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, cmdIDD->getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, cmdIDD->getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, cmdIDD->getConstantIndirectUrbEntryReadLength());
}

HWTEST_F(EnqueueCopyBufferTest, PipelineSelect) {
    enqueueCopyBuffer<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWTEST_F(EnqueueCopyBufferTest, MediaVFEState) {
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    enqueueCopyBuffer<FamilyType>();

    auto *cmd = (MEDIA_VFE_STATE *)cmdMediaVfeState;
    ASSERT_NE(nullptr, cmd);

    // Verify we have a valid length
    EXPECT_EQ(pDevice->getHardwareInfo().pSysInfo->ThreadCount, cmd->getMaximumNumberOfThreads());
    EXPECT_NE(0u, cmd->getNumberOfUrbEntries());
    EXPECT_NE(0u, cmd->getUrbEntryAllocationSize());

    // Generically validate this command
    FamilyType::PARSE::template validateCommand<MEDIA_VFE_STATE *>(cmdList.begin(), itorMediaVfeState);
}

HWTEST_F(EnqueueCopyBufferTest, argumentZeroShouldMatchSourceAddress) {
    enqueueCopyBuffer<FamilyType>();

    // Extract the kernel used
    MultiDispatchInfo multiDispatchInfo;
    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                          pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(*kernel, 0u, pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0));

    EXPECT_EQ((void *)((uintptr_t)srcBuffer->getGraphicsAllocation()->getGpuAddress()), *pArgument);
}

HWTEST_F(EnqueueCopyBufferTest, argumentOneShouldMatchDestAddress) {
    enqueueCopyBuffer<FamilyType>();

    // Extract the kernel used
    MultiDispatchInfo multiDispatchInfo;
    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                          pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    // Determine where the argument is
    auto pArgument = (void **)getStatelessArgumentPointer<FamilyType>(*kernel, 1u, pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0));

    EXPECT_EQ((void *)((uintptr_t)dstBuffer->getGraphicsAllocation()->getGpuAddress()), *pArgument);
}
