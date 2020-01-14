/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/command_queue/enqueue_copy_buffer_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/gen_common/gen_commands_common_validation.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_buffer.h"

#include "reg_configs_common.h"

#include <memory>

using namespace NEO;

HWTEST_F(EnqueueCopyBufferTest, GivenNullSrcMemObjWhenCopyingBufferThenClInvalidMemObjectErrorIsReturned) {
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

HWTEST_F(EnqueueCopyBufferTest, GivenNullDstMemObjWhenCopyingBufferThenClInvalidMemObjectErrorIsReturned) {
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

HWTEST_F(EnqueueCopyBufferTest, GivenInvalidMemoryLocationWhenCopyingBufferThenClInvalidValueErrorIsReturned) {
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

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenTaskCountIsAlignedWithCsr) {
    //this test case assumes IOQ
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.taskCount = pCmdQ->taskCount + 100;
    csr.taskLevel = pCmdQ->taskLevel + 50;

    enqueueCopyBuffer();
    EXPECT_EQ(csr.peekTaskCount(), pCmdQ->taskCount);
    EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenGpgpuWalkerIsCorrect) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    enqueueCopyBufferAndParse<FamilyType>();

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
    uint64_t simdMask = maxNBitValue(simd);

    // Mask off lanes based on the execution masks
    auto laneMaskRight = cmd->getRightExecutionMask() & simdMask;
    auto lanesPerThreadX = 0;
    while (laneMaskRight) {
        lanesPerThreadX += laneMaskRight & 1;
        laneMaskRight >>= 1;
    }
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenTaskLevelIsIncremented) {
    auto taskLevelBefore = pCmdQ->taskLevel;

    enqueueCopyBuffer();
    EXPECT_GT(pCmdQ->taskLevel, taskLevelBefore);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenCommandsAreAdded) {
    auto usedCmdBufferBefore = pCS->getUsed();

    enqueueCopyBuffer();
    EXPECT_NE(usedCmdBufferBefore, pCS->getUsed());
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenIndirectDataGetsAdded) {
    auto dshBefore = pDSH->getUsed();
    auto iohBefore = pIOH->getUsed();
    auto sshBefore = pSSH->getUsed();

    enqueueCopyBuffer();

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                     pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();

    EXPECT_TRUE(UnitTestHelper<FamilyType>::evaluateDshUsage(dshBefore, pDSH->getUsed(), kernel));
    EXPECT_NE(iohBefore, pIOH->getUsed());
    if (kernel->requiresSshForBuffers()) {
        EXPECT_NE(sshBefore, pSSH->getUsed());
    }
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferStatelessThenStatelessKernelIsUsed) {

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBufferStateless,
                                                                                                     pCmdQ->getContext(), pCmdQ->getDevice());

    ASSERT_NE(nullptr, &builder);
    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer.get();
    dc.dstMemObj = dstBuffer.get();
    dc.srcOffset = {EnqueueCopyBufferTraits::srcOffset, 0, 0};
    dc.dstOffset = {EnqueueCopyBufferTraits::dstOffset, 0, 0};
    dc.size = {EnqueueCopyBufferTraits::size, 0, 0};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    EXPECT_TRUE(kernel->getKernelInfo().patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers);
    EXPECT_FALSE(kernel->getKernelInfo().kernelArgInfo[0].pureStatefulBufferAccess);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenL3ProgrammingIsCorrect) {
    enqueueCopyBufferAndParse<FamilyType>();
    validateL3Programming<FamilyType>(cmdList, itorWalker);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferTest, WhenEnqueueIsDoneThenStateBaseAddressIsProperlyProgrammed) {
    enqueueCopyBufferAndParse<FamilyType>();
    auto &ultCsr = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    validateStateBaseAddress<FamilyType>(ultCsr.getMemoryManager()->getInternalHeapBaseAddress(ultCsr.rootDeviceIndex),
                                         pDSH, pIOH, pSSH, itorPipelineSelect, itorWalker, cmdList, 0llu);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenMediaInterfaceDescriptorLoadIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferAndParse<FamilyType>();

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

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenInterfaceDescriptorDataIsCorrect) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    enqueueCopyBufferAndParse<FamilyType>();

    auto cmdIDD = (INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;
    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;

    // Validate the kernel start pointer.  Technically, a kernel can start at address 0 but let's force a value.
    auto kernelStartPointer = ((uint64_t)cmdIDD->getKernelStartPointerHigh() << 32) + cmdIDD->getKernelStartPointer();
    EXPECT_LE(kernelStartPointer, cmdSBA->getInstructionBufferSize() * MemoryConstants::pageSize);

    EXPECT_NE(0u, cmdIDD->getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_NE(0u, cmdIDD->getCrossThreadConstantDataReadLength());
    EXPECT_NE(0u, cmdIDD->getConstantIndirectUrbEntryReadLength());
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenNumberOfPipelineSelectsIsOne) {
    enqueueCopyBufferAndParse<FamilyType>();
    int numCommands = getNumberOfPipelineSelectsThatEnablePipelineSelect<FamilyType>();
    EXPECT_EQ(1, numCommands);
}

HWCMDTEST_F(IGFX_GEN8_CORE, EnqueueCopyBufferTest, WhenCopyingBufferThenMediaVfeStateIsSetCorrectly) {
    enqueueCopyBufferAndParse<FamilyType>();
    validateMediaVFEState<FamilyType>(&pDevice->getHardwareInfo(), cmdMediaVfeState, cmdList, itorMediaVfeState);
}

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenArgumentZeroMatchesSourceAddress) {
    enqueueCopyBufferAndParse<FamilyType>();

    // Extract the kernel used
    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                     pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
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

HWTEST_F(EnqueueCopyBufferTest, WhenCopyingBufferThenArgumentOneMatchesDestinationAddress) {
    enqueueCopyBufferAndParse<FamilyType>();

    // Extract the kernel used
    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                     pCmdQ->getContext(), pCmdQ->getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
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

struct EnqueueCopyBufferHw : public ::testing::Test {

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        context.reset(new MockContext(device.get()));
        dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> dstBuffer;
    MockBuffer srcBuffer;
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    uint64_t smallSize = 4ull * MemoryConstants::gigaByte - 1;
};

using EnqueueCopyBufferStatelessTest = EnqueueCopyBufferHw;

HWTEST_F(EnqueueCopyBufferStatelessTest, givenBuffersWhenCopyingBufferStatelessThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateless<FamilyType>>(context.get(), device.get());
    srcBuffer.size = static_cast<size_t>(bigSize);
    auto retVal = cmdQ->enqueueCopyBuffer(
        &srcBuffer,
        dstBuffer.get(),
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

using EnqueueCopyBufferStatefulTest = EnqueueCopyBufferHw;

HWTEST_F(EnqueueCopyBufferStatefulTest, givenBuffersWhenCopyingBufferStatefulThenSuccessIsReturned) {
    auto cmdQ = std::make_unique<CommandQueueStateful<FamilyType>>(context.get(), device.get());
    srcBuffer.size = static_cast<size_t>(smallSize);
    auto retVal = cmdQ->enqueueCopyBuffer(
        &srcBuffer,
        dstBuffer.get(),
        0,
        0,
        sizeof(float),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}
